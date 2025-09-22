#pragma once
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/KeyRegistry.h"
#include "../../../Runner/Breakpoints/BPRegistry.h"
#include "BattleOutcome.h"

using namespace simcore;

namespace phase::battle::runner {

    static constexpr BPKey BP_BattleLoadComplete = bp::battle::BattleLoadComplete;
    static constexpr BPKey BP_BattleAcceptInput = bp::battle::TurnInputs;
    static constexpr BPKey BP_Victory = bp::battle::EndBattleVictory;
    static constexpr BPKey BP_Defeat = bp::battle::EndBattleDefeat;

    static constexpr keys::KeyId DW_Outcome = keys::core::DW_RUN_OUTCOME_CODE;
    static constexpr keys::KeyId Battle_Outcome = keys::battle::BATTLE_OUTCOME;

    static const std::string LabelA = "A";
    static const std::string LabelB = "B";
    static const std::string LabelADV = "ADV";
    static const std::string LabelVictory = "RET_SUCCESS";
    static const std::string LabelDefeat = "RET_FAILURE";
    static const std::string LabelPredFail = "RET_PRED_FAILURE";
    static const std::string LabelMaterializeFail = "RET_PLAN_MAT_FAILURE";
    static const std::string LabelDWErr = "RET_DW_RUN_ERROR";

    inline PhaseScript MakeBattleRunnerProgram()
    {
        using simcore::battle::Outcome;
        
        PhaseScript ps{};
        ps.canonical_bp_keys = { BP_BattleAcceptInput, BP_Victory, BP_Defeat, BP_BattleLoadComplete };

        ps.ops.push_back(OpArmPhaseBps());
        ps.ops.push_back(OpArmBpsFromPredTable());
        ps.ops.push_back(OpLoadSnapshot());

        ps.ops.push_back(OpSetU32(keys::battle::ACTIVE_TURN, 0));
        ps.ops.push_back(OpApplyInputFrom(keys::battle::INITIAL_INPUT));
        ps.ops.push_back(OpGoto(LabelB));  // Going to LabelB so that we can run until turn inputs checking that the initial battle state is favorable (might need to check turn_type at start of battle)

        // ============  Label A  ===================
        // Run all inputs
        ps.ops.push_back(OpLabel(LabelA));

        // Get context and build a plan - fail fast if plan fails
        ps.ops.push_back(OpGetBattleContext());
        ps.ops.push_back(OpBuildTurnInputFromActions());
        ps.ops.push_back(OpGotoIf(keys::battle::PLAN_MATERIALIZE_ERR, PSCmp::NE, 0, LabelMaterializeFail));

        // apply input plan
        ps.ops.push_back(OpApplyPlanFrameFrom(keys::battle::ACTIVE_TURN));
        ps.ops.push_back(OpGotoIf(keys::core::PLAN_DONE, PSCmp::EQ, 1, LabelB));
        ps.ops.push_back(OpGoto(LabelA));

        // ============  Label B  ===================
        ps.ops.push_back(OpLabel(LabelB));

        // Run to bp, check predicates, record progress
        ps.ops.push_back(OpRunUntilBp());
        ps.ops.push_back(OpGotoIf(DW_Outcome, PSCmp::NE, 0, LabelDWErr));
        ps.ops.push_back(OpEvalPredicatesAtHitBP()); // Sets whether all predicates passed into keys::core::PRED_ALL_PASSED
        ps.ops.push_back(OpRecordProgressAtBP());

        // Check exit conditions
        ps.ops.push_back(OpGotoIf(keys::core::RUN_HIT_BP_KEY, PSCmp::EQ, (uint32_t)BP_Victory, LabelVictory));
        ps.ops.push_back(OpGotoIf(keys::core::RUN_HIT_BP_KEY, PSCmp::EQ, (uint32_t)BP_Defeat, LabelDefeat));
        ps.ops.push_back(OpGotoIf(keys::core::PRED_ALL_PASSED, PSCmp::EQ, (uint32_t)0, LabelPredFail));

        
        // If we are not to the next input bp, keep running
        ps.ops.push_back(OpGotoIf(keys::core::RUN_HIT_BP_KEY, PSCmp::EQ, (uint32_t)BP_BattleLoadComplete, LabelADV));
        ps.ops.push_back(OpGotoIf(keys::core::RUN_HIT_BP_KEY, PSCmp::NE, (uint32_t)BP_BattleAcceptInput, LabelB));

        ps.ops.push_back(OpGotoIfKeys(keys::battle::ACTIVE_TURN, PSCmp::LE, keys::battle::LAST_TURN, LabelADV));
        ps.ops.push_back(OpReturnResult(Battle_Outcome, (uint32_t)Outcome::TurnsExhausted));

        // ============  Label ADV  ===================
        ps.ops.push_back(OpLabel(LabelADV));
        ps.ops.push_back(OpAddU32(keys::battle::ACTIVE_TURN, 1));
        ps.ops.push_back(OpCapturePredBaselines());
        ps.ops.push_back(OpGoto(LabelA));

        // ============  Label Victory  ===================
        ps.ops.push_back(OpLabel(LabelVictory));
        ps.ops.push_back(OpReturnResult(Battle_Outcome, (uint32_t)Outcome::Victory));

        // ============  Label Defeat  ===================
        ps.ops.push_back(OpLabel(LabelDefeat));
        ps.ops.push_back(OpReturnResult(Battle_Outcome, (uint32_t)Outcome::Defeat));

        // ============  Label Predicate Failure  ===================
        ps.ops.push_back(OpLabel(LabelPredFail));
        ps.ops.push_back(OpReturnResult(Battle_Outcome, (uint32_t)Outcome::PredFailure));

        // ============  Label Plan Mismatch  ===================
        ps.ops.push_back(OpLabel(LabelMaterializeFail));
        ps.ops.push_back(OpReturnResult(Battle_Outcome, (uint32_t)Outcome::PlanMaterializeFailure));

        // ============  Label Dolphin Wrapper Run Error  ===================
        ps.ops.push_back(OpLabel(LabelDWErr));
        ps.ops.push_back(OpReturnResult(Battle_Outcome, (uint32_t)Outcome::DWRunErr));

        return ps;
    }
} // namespace simcore::battle