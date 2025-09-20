#include "PlanWriter.h"

using simcore::GCInputFrame;
using simcore::InputPlan;

namespace soa::battle::actions {

    static inline void push_btn(InputPlan& p, uint16_t btn) {
        GCInputFrame f{}; f.buttons = btn; p.push_back(f);
        p.push_back(GCInputFrame{}); // enforce neutral between identical presses
    }

    PlanWriter::PlanWriter(const soa::battle::ctx::BattleContext& bc) : bc_(bc) {}

    void PlanWriter::tapA(InputPlan& p) { push_btn(p, simcore::GC_A); }
    void PlanWriter::tapB(InputPlan& p) { push_btn(p, simcore::GC_B); }
    void PlanWriter::tapUp(InputPlan& p) { push_btn(p, simcore::GC_DU); }
    void PlanWriter::tapDown(InputPlan& p) { push_btn(p, simcore::GC_DD); }
    void PlanWriter::neutral(InputPlan& p, uint32_t n) { for (uint32_t i = 0; i < n; ++i) p.push_back(GCInputFrame{}); }

    void PlanWriter::navMainTo(InputPlan& p, uint8_t dst) {
        if (dst > 6) dst = 6;
        while (cmd_index_ < dst) { tapDown(p); neutral(p, 2); ++cmd_index_; } // main menu: 2-frame settle
        while (cmd_index_ > dst) { tapUp(p);   neutral(p, 2); --cmd_index_; }
    }

    int PlanWriter::firstAliveEnemyIndex() const {
        for (int i = 4; i < 12; ++i) if (bc_.slots[i].present && !bc_.slots[i].is_player) return i - 4;
        return -1;
    }

    int PlanWriter::currentTargetIndex() const {
        // TODO-WRAP: assume default = first alive for now
        return firstAliveEnemyIndex();
    }

    int PlanWriter::resolveRequestedTargetIndex(uint32_t mask) const {
        int base = firstAliveEnemyIndex();
        if (base < 0) return -1;
        if (!mask) return base;
        for (int i = 4; i < 12; ++i) {
            if (bc_.slots[i].present && !bc_.slots[i].is_player) {
                uint32_t logical = bc_.slots[i].id;
                if (mask & (1u << (logical & 31u))) return i - 4;
            }
        }
        return -1;
    }

    void PlanWriter::navTargetTo(InputPlan& p, int cur, int dst) {
        if (cur < 0 || dst < 0) return;
        const int n = 8; // up to 8 enemy slots (wrap allowed)
        const int down = (dst - cur + n) % n;
        const int up = (cur - dst + n) % n;
        if (down <= up) { for (int i = 0; i < down; ++i) tapDown(p); }
        else { for (int i = 0; i < up;  ++i) tapUp(p); }
    }

    bool PlanWriter::attack(InputPlan& p, const ActionParameters& ap, MaterializeErr& err) {
        navMainTo(p, 3);
        tapA(p); neutral(p, 1); // enter targets, 1-frame animation
        const int cur = currentTargetIndex();
        const int dst = resolveRequestedTargetIndex(ap.target_mask);
        if (dst < 0) { err = MaterializeErr::NoValidTarget; return false; }
        navTargetTo(p, cur, dst);
        tapA(p); // confirm target; game returns to main menu highlight (assume 3)
        cmd_index_ = 3;
        return true;
    }

    bool PlanWriter::defend(InputPlan& p, MaterializeErr&) {
        navMainTo(p, 2);
        tapA(p);
        return true;
    }

    bool PlanWriter::focus(InputPlan& p, MaterializeErr& err) {
        // TODO-RES: if insufficient resource, set err and return false
        navMainTo(p, 6);
        tapA(p);
        return true;
    }

    bool PlanWriter::fake_attack(InputPlan& p, const TurnPlan& ap) {
        for (uint8_t i = 0; i < ap.fake_attack_count; ++i) {
            navMainTo(p, 3);
            tapA(p); neutral(p, 1); // open targets
            tapB(p); neutral(p, 1); // back out
        }
        navMainTo(p, 3);
        return true;
    }

    bool PlanWriter::buildTurn(const TurnPlan& plan, InputPlan& out, MaterializeErr& err) {
        out.clear();
        err = MaterializeErr::OK;
        if (!fake_attack(out, plan)) return false;
        for (const auto& ac : plan.spec ) {
            actor_slot_ = ac.actor_slot; // tracked for future, if actor-specific behaviors needed
            switch (ac.macro) {
            case BattleAction::Attack: if (!attack(out, ac.params, err)) return false; break;
            case BattleAction::Defend: if (!defend(out, err)) return false; break;
            case BattleAction::Focus:  if (!focus(out, err)) return false; break;
            default: return false;
            }
        }
        return true;
    }

} // namespace soa::battle::actions
