
/* DERIVED buffer layout (offsets as BASE_VA):
0x0000..0x000B : TurnOrderIdx_[PC0..PC3, Enemy0..Enemy7] (U8 each)
0x0010         : CurrentTurn (U32)
0x0014         : OrderSize  (U32)
*/
#define ADDR_TABLE_DERIVED_BATTLE(X) \
  X(derived_battle, TurnOrderIdx_base,   DERIVED, 0x0000) \
  X(derived_battle, CurrentTurn,         DERIVED, 0x0010) \
  X(derived_battle, TurnOrderSize,       DERIVED, 0x0014)
