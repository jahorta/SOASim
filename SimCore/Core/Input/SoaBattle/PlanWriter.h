#pragma once
#include <cstdint>
#include <vector>
#include "../InputPlan.h"
#include "../../Memory/Soa/Battle/BattleContext.h"
#include "ActionTypes.h"

namespace soa::battle::actions {

    enum class MaterializeErr : uint32_t {
        OK = 0,
        NoValidTarget = 1,
        NotEnoughResource = 2,
        InvalidNavigation = 3,
        OutOfTurns = 4,
        BadBlob = 5
    };

    class PlanWriter {
    public:
        PlanWriter(const soa::battle::ctx::BattleContext& bc);

        bool buildTurn(const TurnPlan& plan, simcore::InputPlan& out, MaterializeErr& err);

    private:
        soa::battle::ctx::BattleContext bc_;
        uint8_t cmd_index_ = 3; // Attack
        uint8_t actor_slot_ = 0;

        void tapA(simcore::InputPlan& p);
        void tapB(simcore::InputPlan& p);
        void tapUp(simcore::InputPlan& p);
        void tapDown(simcore::InputPlan& p);
        void neutral(simcore::InputPlan& p, uint32_t n);

        void navMainTo(simcore::InputPlan& p, uint8_t dst); // main menu, no wrap, +neutral(2) after each move
        bool attack(simcore::InputPlan& p, const ActionParameters& ap, MaterializeErr& err);
        bool defend(simcore::InputPlan& p, MaterializeErr& err);
        bool focus(simcore::InputPlan& p, MaterializeErr& err);
        bool fake_attack(simcore::InputPlan& p, const TurnPlan& ap);

        // submenu targeting helpers (wrap allowed)
        int currentTargetIndex() const; // TODO-WRAP: derive or assume 0
        int firstAliveEnemyIndex() const; // uses bc_; assumes top->bottom
        int resolveRequestedTargetIndex(uint32_t mask) const; // single-target for now

        void navTargetTo(simcore::InputPlan& p, int cur, int dst); // wrap-aware

        // TODO-RES: resource checks (Focus/SP gating)
    };

} // namespace soa::battle::actions
