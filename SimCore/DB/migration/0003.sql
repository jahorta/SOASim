BEGIN;

-- Bump schema version to 3
CREATE TABLE IF NOT EXISTS schema_version (
  version    INTEGER NOT NULL,
  applied_at INTEGER NOT NULL
);
DELETE FROM schema_version;
INSERT INTO schema_version(version, applied_at)
VALUES (3, strftime('%s','now'));

----------------------------------------------------------------------
-- 1) Battle plan atoms (deduplicated, versioned ActionPlan wire)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS battle_plan_atom (
  id            INTEGER PRIMARY KEY,
  wire_version  INTEGER NOT NULL,   -- ActionPlan wire codec version
  wire_bytes    BLOB    NOT NULL,   -- canonical encoding of ActionPlan (exact bytes sent to worker)
  -- Optional denormalized hints (not part of identity)
  actor_slot    INTEGER,            -- 0..3
  action_type   INTEGER,            -- BattleAction enum
  param_item_id INTEGER,            -- when UseItem, else NULL
  target_slot   INTEGER,            -- params.target_slot (optional cache)
  UNIQUE (wire_version, wire_bytes)
);
CREATE INDEX IF NOT EXISTS ix_battle_plan_atom_type_actor
  ON battle_plan_atom(action_type, actor_slot);

----------------------------------------------------------------------
-- 2) Battle plan turn hierarchy under explorer_settings
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS battle_plan_turn (
  settings_id        INTEGER NOT NULL REFERENCES explorer_settings(id) ON DELETE CASCADE,
  turn_index         INTEGER NOT NULL,    -- 0..N
  max_fake_atk_count INTEGER NOT NULL,
  PRIMARY KEY (settings_id, turn_index)
);

CREATE TABLE IF NOT EXISTS battle_plan_turn_actor (
  settings_id  INTEGER NOT NULL REFERENCES explorer_settings(id) ON DELETE CASCADE,
  turn_index   INTEGER NOT NULL,
  actor_index  INTEGER NOT NULL,          -- PC slot index
  atom_id      INTEGER NOT NULL REFERENCES battle_plan_atom(id) ON DELETE RESTRICT,
  PRIMARY KEY (settings_id, turn_index, actor_index),
  FOREIGN KEY (settings_id, turn_index)
    REFERENCES battle_plan_turn(settings_id, turn_index) ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 3) Address programs (reusable, versioned blobs)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS address_program (
  id                         INTEGER PRIMARY KEY,
  program_version            INTEGER NOT NULL,   -- addr program VM/opset version
  prog_bytes                 BLOB    NOT NULL,   -- raw bytecode as sent to worker
  derived_buffer_version     INTEGER,            -- e.g., DerivedBattleBuffer version
  derived_buffer_schema_hash TEXT,               -- optional hash for buffer layout
  soa_structs_hash           TEXT,               -- hash of soa_structs.h
  description                TEXT
);
CREATE UNIQUE INDEX IF NOT EXISTS ux_address_program_nk
  ON address_program(
       program_version,
       prog_bytes,
       COALESCE(derived_buffer_version,0),
       COALESCE(derived_buffer_schema_hash,''),
       COALESCE(soa_structs_hash,'')
     );

----------------------------------------------------------------------
-- 4) Predicate specs (atomic; optional LHS/RHS address programs)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS predicate_spec (
  id           INTEGER PRIMARY KEY,
  spec_version INTEGER NOT NULL,       -- schema/encoding version of this row
  pred_id      INTEGER NOT NULL,       -- your Spec::id (stable numeric)
  required_bp  INTEGER NOT NULL,
  kind         INTEGER NOT NULL,       -- PredKind (0=ABS,1=DELTA)
  width        INTEGER NOT NULL,       -- 1,2,4,8
  cmp_op       INTEGER NOT NULL,       -- CmpOp (0..5)
  flags        INTEGER NOT NULL,       -- PredFlag bitfield
  lhs_addr     INTEGER NOT NULL,       -- 0 if unused (legacy absolute VA)
  lhs_key      INTEGER,                -- addr::AddrKey numeric (NULL if none)
  rhs_value    INTEGER NOT NULL,       -- immediate RHS when not key/prog
  rhs_key      INTEGER,                -- addr::AddrKey numeric
  turn_mask    INTEGER NOT NULL,       -- 32-bit mask
  lhs_prog_id  INTEGER REFERENCES address_program(id) ON DELETE RESTRICT,
  rhs_prog_id  INTEGER REFERENCES address_program(id) ON DELETE RESTRICT,
  desc         TEXT
);
CREATE INDEX IF NOT EXISTS ix_predicate_spec_pred ON predicate_spec(pred_id);
CREATE INDEX IF NOT EXISTS ix_predicate_spec_bp   ON predicate_spec(required_bp);

----------------------------------------------------------------------
-- 5) Explorer settings <-> predicate specs (ordered list)
----------------------------------------------------------------------

-- If an older explorer_settings_predicate exists from 0001, replace it:
DROP TABLE IF EXISTS explorer_settings_predicate;

CREATE TABLE explorer_settings_predicate (
  settings_id  INTEGER NOT NULL REFERENCES explorer_settings(id) ON DELETE CASCADE,
  ordinal      INTEGER NOT NULL,
  predicate_id INTEGER NOT NULL REFERENCES predicate_spec(id) ON DELETE RESTRICT,
  PRIMARY KEY (settings_id, ordinal)
);
CREATE INDEX IF NOT EXISTS ix_settings_predicate_pid
  ON explorer_settings_predicate(settings_id, predicate_id);

COMMIT;
