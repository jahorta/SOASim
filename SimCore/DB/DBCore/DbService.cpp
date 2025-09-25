#include "DbService.h"

#include "MigrationRunner.h"
#include "MigrationRunner_Embedded.h"

#include <chrono>
#include <exception>
#include <sqlite3.h>

namespace simcore {
    namespace db {

        DBService::DBService() = default;
        DBService::~DBService() { stop(); }

        DBService& DBService::instance() { static DBService inst; return inst; }

        void DBService::start(const std::string& db_path) {
            bool expected = false;
            if (!m_running.compare_exchange_strong(expected, true)) return;

            m_env = DbEnv::open(db_path);
            try { ApplyEmbeddedMigrations(*m_env); }
            catch (...) {}

            m_worker = std::thread([this]() { workerLoop(); });
        }

        void DBService::stop() {
            if (!m_running.exchange(false)) return;
            { std::lock_guard<std::mutex> lock(m_mutex); }
            m_hasTask.notify_all();
            if (m_worker.joinable()) m_worker.join();
            m_env.reset();
            for (auto& q : m_queues) while (!q.empty()) q.pop();
            m_size = 0;
        }

        void DBService::record_submit(Priority prio) {
            std::lock_guard<std::mutex> g(m_metrics_mtx);
            ++m_stats.submitted;
            if (prio == Priority::High) ++m_stats.queued_high;
            else ++m_stats.queued_normal;
        }

        void DBService::record_dequeue(Priority prio, uint64_t wait_us) {
            std::lock_guard<std::mutex> g(m_metrics_mtx);
            if (prio == Priority::High) {
                if (m_stats.queued_high) --m_stats.queued_high;
            }
            else {
                if (m_stats.queued_normal) --m_stats.queued_normal;
            }
            // EMA for wait
            const double alpha = 0.1;
            m_stats.avg_wait_us = (1.0 - alpha) * m_stats.avg_wait_us + alpha * static_cast<double>(wait_us);
        }

        void DBService::record_result(bool ok, bool /*did_retry*/, uint64_t exec_us) {
            std::lock_guard<std::mutex> g(m_metrics_mtx);
            if (ok) ++m_stats.completed; else ++m_stats.failed;
            const double alpha = 0.1;
            m_stats.avg_exec_us = (1.0 - alpha) * m_stats.avg_exec_us + alpha * static_cast<double>(exec_us);
        }

        DBService::Stats DBService::stats() const {
            std::lock_guard<std::mutex> g(m_metrics_mtx);
            return m_stats;
        }

        DBService::QueuedTask DBService::fetch_qt() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_hasTask.wait(lock, [&]() { return !m_running || m_size > 0; });
            if (!m_running && m_size == 0) return {};
            DBService::QueuedTask qt{};
            for (std::size_t i = 0; i < m_queues.size(); ++i) {
                auto& q = m_queues[i];
                if (!q.empty()) {
                    qt = q.front();
                    q.pop();
                    --m_size;
                    break;
                }
            }
            m_notFull.notify_one();
            return qt;
        }

        void DBService::workerLoop() {
            while (m_running) {
                auto qt = fetch_qt();
                if (!qt.task) break;

                const auto now = std::chrono::steady_clock::now();
                const auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(now - qt.enq_tp).count();
                record_dequeue(qt.prio, static_cast<uint64_t>(wait_us));

                const auto exec_start = std::chrono::steady_clock::now();
                bool ok = true;
                try {
                    if (!m_env) throw std::runtime_error("DBService not initialized");

                    // For OpType::Write, callers often already start Tx inside their lambda.
                    // We keep your previous behavior: if type==Write, we create a Tx guard.
                    if (qt.task->type == OpType::Write) {
                        DbEnv::Tx tx(*m_env);
                        qt.task->execute(*m_env);
                        try { tx.commit(); }
                        catch (...) { ok = false; }
                    }
                    else {
                        qt.task->execute(*m_env);
                    }
                }
                catch (...) {
                    ok = false;
                }
                const auto exec_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - exec_start).count();
                record_result(ok, false, static_cast<uint64_t>(exec_us));
            }
        }

        // ===== template bodies =====

        template <typename T>
        std::future<T> DBService::submit(OpType type, Priority prio, std::function<T(DbEnv&)> fn) {
            auto task = std::make_shared<Task<T>>(type, prio, std::move(fn));
            auto fut = task->promise.get_future();
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_notFull.wait(lock, [this]() { return m_running && m_size < m_maxQueue; });
                record_submit(prio);
                m_queues[static_cast<std::size_t>(prio)].push(QueuedTask{ task, std::chrono::steady_clock::now(), prio });
                ++m_size;
                m_stats.peak_queue = (std::max)(m_stats.peak_queue, static_cast<uint64_t>(m_size));
            }
            m_hasTask.notify_one();
            return fut;
        }

        template <typename T>
        std::future<DbResult<T>> DBService::submit_res(OpType type, Priority prio, RetryPolicy policy,
            std::function<DbResult<T>(DbEnv&)> fn) {
            // Wrap DbResult<T> into a Task<DbResult<T>>
            auto exec = [this, type, policy, fn = std::move(fn)](DbEnv& env) -> DbResult<T> {
                int attempt = 0;
                auto backoff = policy.initial_backoff;
                while (true) {
                    ++attempt;
                    DbResult<T> r;
                    try {
                        r = fn(env);
                    }
                    catch (...) {
                        // Map unknown exceptions to Unknown error
                        r = DbResult<T>::Err(DbError{ DbErrorKind::Unknown, 0, "exception" });
                    }
                    if (r.ok) return r;

                    const auto k = r.error.kind;
                    const bool retryable = (k == DbErrorKind::Busy || k == DbErrorKind::Locked);
                    if (!policy.enabled() || !retryable || attempt >= policy.max_attempts) {
                        return r;
                    }
                    {
                        std::lock_guard<std::mutex> g(m_metrics_mtx);
                        ++m_stats.retried;
                    }
                    std::this_thread::sleep_for(backoff);
                    auto next_us = static_cast<int64_t>(backoff.count() * policy.backoff_multiplier);
                    if (next_us > policy.max_backoff.count()) next_us = policy.max_backoff.count();
                    backoff = std::chrono::milliseconds(next_us);
                }
                };
            return submit<DbResult<T>>(type, prio, std::move(exec));
        }

    } // namespace db
} // namespace simcore
