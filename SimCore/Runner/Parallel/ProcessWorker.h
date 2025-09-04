#pragma once
#include <string>
#include <thread>
#include <windows.h>
#include "../../Core/Input/InputPlan.h"
#include "../Script/PhaseScriptVM.h"  // for PSResult
#include "TSQueue.h"
#include "PRTypes.h"
#include "../Breakpoints/BPCore.h"


namespace simcore {

	struct ProcStartParams {
		size_t worker_id{ 0 };
		std::string exe_path;     // path to SimCoreSandbox.exe
		std::string iso_path;
		std::string savestate_path;
		std::string qt_base_dir;
		std::string user_dir;     // unique per worker
		uint32_t timeout_ms{ 10000 };
	};

	class ProcessWorker {
	public:
		ProcessWorker() = default;
		~ProcessWorker();

		bool start(ProcStartParams& p, TSQueue<PRResult>* out_queue);
		bool send_job(uint64_t job_id, uint64_t epoch, const GCInputFrame& f);
		void stop();

		bool is_ready()  const { return ready_received_.load() && ready_ok_.load(); }
		bool is_failed() const { return ready_received_.load() && !ready_ok_.load(); }
		uint32_t ready_error() const { return ready_error_.load(); }
		HANDLE process_handle() const { return hProcess; }

		bool try_acquire_slot() {
			bool expected = false;
			return busy_.compare_exchange_strong(expected, true, std::memory_order_acq_rel);
		}
		void release_slot() { busy_.store(false, std::memory_order_release); }
		bool has_slot() const { return !busy_.load(std::memory_order_acquire); }

	private:
		void reader_thread();

		HANDLE hChildStd_IN_Wr{ NULL };  // parent writes jobs here
		HANDLE hChildStd_OUT_Rd{ NULL }; // parent reads results here
		HANDLE hProcess{ NULL };
		HANDLE hThread{ NULL };
		unsigned long dwProcessId{ 0 };

		std::thread reader_;
		TSQueue<PRResult>* out_{ nullptr };
		size_t id_{ 0 };
		std::atomic<bool> running_{ false };
		
		std::atomic<bool> busy_{ false };           // true if a job is in-flight
		std::atomic<bool> ready_received_{ false }; // we saw MSG_READY
		std::atomic<bool> ready_ok_{ false };       // MSG_READY.ok
		std::atomic<uint32_t> ready_error_{ 0 };    // MSG_READY.error
	};

} // namespace simcore
