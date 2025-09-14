#pragma once
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/VMCoreKeys.reg.h"
#include "BattleOutcome.h"
#include "BattleRunnerKeys.reg.h"
#include "../../../Runner/Breakpoints/BPRegistry.h"

    namespace simcore::battle {

        static constexpr BPKey BP_BattleAcceptInput = bp::battle::TurnInputs;
        static constexpr BPKey BP_Victory = bp::battle::EndBattle;
        static constexpr BPKey BP_Defeat = bp::battle::EndBattle;

        inline PhaseScript MakeBattleRunnerProgram()
        {
            PhaseScript ps{};
            ps.canonical_bp_keys = { BP_BattleAcceptInput, BP_Victory, BP_Defeat };

            ps.ops.push_back(OpArmPhaseBps());
            ps.ops.push_back(OpArmBpsFromPredTable());
            ps.ops.push_back(OpLoadSnapshot());

            ps.ops.push_back(OpApplyInputFrom(keys::battle::INITIAL_INPUT));
            ps.ops.push_back(OpRunUntilBp());

            ps.ops.push_back(OpSetU32(keys::battle::ACTIVE_TURN, 0));
            ps.ops.push_back(OpCapturePredBaselines());

            ps.ops.push_back(OpLabel("A"));
            ps.ops.push_back(OpApplyPlanFrameFrom(keys::battle::ACTIVE_TURN));
            ps.ops.push_back(OpGotoIf(keys::core::PLAN_DONE, PSCmp::EQ, 1, "B"));
            ps.ops.push_back(OpGoto("A"));

            ps.ops.push_back(OpLabel("B"));
            ps.ops.push_back(OpRunUntilBp());
            ps.ops.push_back(OpEvalPredicatesAtHitBP());
            ps.ops.push_back(OpRecordProgressAtBP());

            ps.ops.push_back(OpGotoIf(keys::core::RUN_HIT_BP, PSCmp::EQ, (uint32_t)BP_Victory, "RET_SUCCESS"));
            ps.ops.push_back(OpGotoIf(keys::core::RUN_HIT_BP, PSCmp::EQ, (uint32_t)BP_Defeat, "RET_FAILURE"));

            ps.ops.push_back(OpGotoIf(keys::core::RUN_HIT_BP, PSCmp::NE, (uint32_t)BP_BattleAcceptInput, "B"));

            ps.ops.push_back(OpGotoIf(keys::core::PLAN_DONE, PSCmp::EQ, 0, "RET_MISMATCH"));
            ps.ops.push_back(OpGotoIfKeys(keys::battle::ACTIVE_TURN, PSCmp::LT,
                keys::battle::LAST_TURN_IDX, "ADV"));
            ps.ops.push_back(OpReturnResult((uint32_t)Outcome::TurnsExhausted));

            ps.ops.push_back(OpLabel("ADV"));
            ps.ops.push_back(OpAddU32(keys::battle::ACTIVE_TURN, 1));
            ps.ops.push_back(OpCapturePredBaselines());
            ps.ops.push_back(OpGoto("A"));

            ps.ops.push_back(OpLabel("RET_SUCCESS"));
            ps.ops.push_back(OpReturnResult((uint32_t)Outcome::Victory));

            ps.ops.push_back(OpLabel("RET_FAILURE"));
            ps.ops.push_back(OpReturnResult((uint32_t)Outcome::Defeat));

            ps.ops.push_back(OpLabel("RET_MISMATCH"));
            ps.ops.push_back(OpReturnResult((uint32_t)Outcome::PlanMismatch));

            return ps;
        }
    } // namespace simcore::battle