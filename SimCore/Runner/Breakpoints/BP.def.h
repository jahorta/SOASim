#pragma once

// Overworld breakpoints
#define BP_TABLE_OVERWORLD(X) \
  X(overworld, OverworldInit,      501, 0x00000000u, "OverworldInit") \
  X(overworld, StartTravelInputs,  502, 0x00000000u, "StartTravelInputs") \
  X(overworld, RandomEncounter,    503, 0x00000000u, "RandomEncounter") \
  X(overworld, ReachedGoal,        504, 0x00000000u, "ReachedGoal")

// Pre-battle / RNG probe breakpoints
#define BP_TABLE_PREBATTLE(X) \
  X(prebattle, BeforeRandSeedSet,  101, 0x80101e48u, "BeforeRandSeedSet") \
  X(prebattle, AfterRandSeedSet,   102, 0x8000a1dcu, "AfterRandSeedSet")

// Battle breakpoints
#define BP_TABLE_BATTLE(X) \
  X(battle, BattleInit,            201, 0x8000a1dcu, "BattleInit") \
  X(battle, BattleInitComplete,    202, 0x8000a2d4u, "BattleInitComplete") \
  X(battle, TurnInputs,            203, 0x800719dcu, "TurnInputs") \
  X(battle, TurnIsReady,           204, 0x800715ecu, "TurnIsReady")  /* This should be at the end of the turn init fxn which generates enemy instructions and turn order. */\
  X(battle, StartTurn,             205, 0x800715dcu, "StartTurn") \
  X(battle, StartAction,           206, 0x800715dcu, "StartAction") \
  X(battle, EndAction,             207, 0x8007033cu, "EndAction") \
  X(battle, EndTurn,               208, 0x8007033cu, "EndTurn") \
  X(battle, EndBattle,             209, 0xFEEDBEEFu, "EndBattle")

#define BP_TABLE_ALL(X) \
  BP_TABLE_OVERWORLD(X) \
  BP_TABLE_PREBATTLE(X) \
  BP_TABLE_BATTLE(X)
