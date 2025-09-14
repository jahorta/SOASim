#pragma once

// domain, NAME,                  REGION, MODE,     BASE_VA,     WIDTH, /* optional offsets... */
// Offsets semantics for PtrChain: cur=BASE; for each off: cur = *(u32*)cur + off; final VA = cur.

#define ADDR_TABLE_CORE(X) \
  X(core,   RNG_SEED,               MEM1,   Raw,      0x803469A8, U32 /* no offsets */) \
  X(core,   SCT_FILE_NUM,           MEM1,   Raw,      0x80311AC4, U32 /* no offsets */) \
  X(core,   SCT_FILE_LTTR,          MEM1,   Raw,      0x80311AC8, U32 /* no offsets */)

#define ADDR_TABLE_BATTLE(X) \
  /* 12×u32 pointers (PC0..PC3, EC0..EC7) */ \
  X(battle, CombatantInstancesTable, MEM1,  Raw,      0x80309DE4, U32 /* no offsets */) \
  /* 12×u16 IDs parallel to instances */ \
  X(battle, CombatantIdTable,        MEM1,  Raw,      0x80309DCC, U16 /* no offsets */) \
/* Example PtrChain: EnemyDefinition pointer at +0x110 from a CombatantInstance BASE. \
     BASE is dynamic (the instance VA); pass it via resolve_from_base(). */ \
  X(battle, EnemyDefinitionFromInstance, MEM1, PtrChain, 0x00000000, U32, 0x110)

#define ADDR_TABLE_ALL(X) \
  ADDR_TABLE_CORE(X)       \
  ADDR_TABLE_BATTLE(X)
