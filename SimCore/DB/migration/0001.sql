-- Versions / lineage (optional but useful)
CREATE TABLE version_stamp (
  id INTEGER PRIMARY KEY,
  dolphin_build_hash   TEXT NOT NULL,
  wrapper_commit       TEXT NOT NULL,
  simcore_commit       TEXT NOT NULL,
  vm_opcode_table_hash TEXT NOT NULL,
  predicate_catalog_hash TEXT NOT NULL,
  addr_registry_hash   TEXT NOT NULL
);
CREATE UNIQUE INDEX ux_version_stamp_all
  ON version_stamp(dolphin_build_hash,wrapper_commit,simcore_commit,
                   vm_opcode_table_hash,predicate_catalog_hash,addr_registry_hash);

-- Content-addressed objects on disk (zstd/lz4/raw)
CREATE TABLE object_ref (
  id INTEGER PRIMARY KEY,
  kind        TEXT NOT NULL,         -- 'savestate','result_parquet','log','aux', ...
  sha256      TEXT NOT NULL UNIQUE,
  size_bytes  INTEGER NOT NULL,
  compression TEXT NOT NULL,         -- 'none','zstd','lz4'
  path        TEXT NOT NULL,         -- relative under objects/aa/bb/<sha256>
  created_at  INTEGER NOT NULL
);
CREATE INDEX ix_object_ref_kind ON object_ref(kind);

-- Savestates are first-class and reference a stored object
CREATE TABLE savestate (
  id INTEGER PRIMARY KEY,
  sha256           TEXT NOT NULL UNIQUE,         -- of the stored savestate object
  savestate_type   TEXT NOT NULL,                -- 'Battle' or 'Overworld'
  obj_savestate_id INTEGER NOT NULL REFERENCES object_ref(id) ON DELETE RESTRICT,
  source_kind      TEXT NOT NULL,                -- 'manual','TAS_frame','VM_program'
  source_args_kv   TEXT,                         -- optional key=value lines (stable order)
  note             TEXT,
  status           TEXT NOT NULL,                -- 'planned','queued','running','complete','failed','aborted'
  created_at       INTEGER NOT NULL,
  started_at       INTEGER,
  finished_at      INTEGER
);

-- Probes over a savestate
CREATE TABLE seed_probe (
  id INTEGER PRIMARY KEY,
  savestate_id  INTEGER NOT NULL REFERENCES savestate(id) ON DELETE CASCADE,
  params_kv     TEXT,                  -- small human-readable config (key=value\n), optional
  params_hash   TEXT NOT NULL,         -- normalized params hash (no JSON)
  neutral_seed  INTEGER NOT NULL,      -- base RNG seed measured for this probe
  vm_version_id INTEGER NOT NULL REFERENCES version_stamp(id) ON DELETE RESTRICT,
  status        TEXT NOT NULL,         -- lifecycle
  progress_pct  REAL NOT NULL DEFAULT 0.0,
  progress_log  TEXT,                  -- append-only lines: ISO8601Z|pct|msg\n
  created_at    INTEGER NOT NULL,
  started_at    INTEGER,
  finished_at   INTEGER
);
CREATE UNIQUE INDEX ux_seed_probe_nk ON seed_probe(savestate_id, params_hash);
CREATE INDEX ix_seed_probe_savestate ON seed_probe(savestate_id);

-- Individual deltas inside a probe
CREATE TABLE seed_delta (
  id INTEGER PRIMARY KEY,
  probe_id    INTEGER NOT NULL REFERENCES seed_probe(id) ON DELETE CASCADE,
  seed_delta  INTEGER NOT NULL,       -- offset from probe.neutral_seed (signed)
  delta_key   BLOB NOT NULL,          -- raw GCInputFrame bytes (canonicalized)
  segment_kind TEXT NOT NULL,         -- 'Grid' or 'Unique'
  status      TEXT NOT NULL,          -- optional lifecycle for planning/validation
  created_at  INTEGER NOT NULL,
  finished_at INTEGER
);
CREATE UNIQUE INDEX ux_seed_delta_nk ON seed_delta(probe_id, delta_key);
CREATE INDEX ix_seed_delta_probe ON seed_delta(probe_id);
CREATE INDEX ix_seed_delta_probe_segment ON seed_delta(probe_id, segment_kind);
CREATE INDEX ix_seed_delta_probe_seeddelta ON seed_delta(probe_id, seed_delta);

-- Settings differentiate runs without JSON
CREATE TABLE explorer_settings (
  id INTEGER PRIMARY KEY,
  name        TEXT,
  description TEXT,
  created_at  INTEGER NOT NULL
);

-- Ordered list of UI actions in a setting
-- ui_action_code/param should match your stable enum & optional parameter (e.g., item_id)
CREATE TABLE explorer_settings_action (
  settings_id    INTEGER NOT NULL REFERENCES explorer_settings(id) ON DELETE CASCADE,
  ordinal        INTEGER NOT NULL,         -- 0..N in deterministic order
  ui_action_code INTEGER NOT NULL,
  ui_action_param INTEGER,
  PRIMARY KEY (settings_id, ordinal)
);
CREATE INDEX ix_settings_action_code ON explorer_settings_action(settings_id, ui_action_code);

-- Ordered list of predicate atoms in a setting
-- predicate_atom_id should be the stable numeric ID from your predicate catalog (no JSON)
CREATE TABLE explorer_settings_predicate (
  settings_id       INTEGER NOT NULL REFERENCES explorer_settings(id) ON DELETE CASCADE,
  ordinal           INTEGER NOT NULL,
  predicate_atom_id INTEGER NOT NULL,      -- stable ID from your PredicateRecord catalog
  PRIMARY KEY (settings_id, ordinal)
);
CREATE INDEX ix_settings_predicate_atom ON explorer_settings_predicate(settings_id, predicate_atom_id);

-- Explorer runs (artifacts are external; small rollups inline)
CREATE TABLE explorer_run (
  id INTEGER PRIMARY KEY,
  probe_id             INTEGER NOT NULL REFERENCES seed_probe(id) ON DELETE CASCADE,
  delta_id             INTEGER NOT NULL REFERENCES seed_delta(id) ON DELETE CASCADE,
  explorer_settings_id INTEGER NOT NULL REFERENCES explorer_settings(id) ON DELETE RESTRICT,
  ui_config_hash       TEXT NOT NULL,
  vm_program_hash      TEXT NOT NULL,
  status               TEXT NOT NULL,             -- planned/queued/running/complete/failed/aborted
  progress_pct         REAL NOT NULL DEFAULT 0.0,
  progress_log         TEXT,                      -- append-only lines
  rows_estimate        INTEGER,
  obj_result_parquet_id INTEGER REFERENCES object_ref(id) ON DELETE SET NULL,
  obj_log_id            INTEGER REFERENCES object_ref(id) ON DELETE SET NULL,
  obj_aux_id            INTEGER REFERENCES object_ref(id) ON DELETE SET NULL,
  started_at           INTEGER,
  finished_at          INTEGER
);
-- Natural key: same probe+delta+settings+program/config => same run identity
CREATE UNIQUE INDEX ux_explorer_run_nk
  ON explorer_run(probe_id, delta_id, explorer_settings_id, ui_config_hash, vm_program_hash);
CREATE INDEX ix_explorer_run_delta ON explorer_run(delta_id);
CREATE INDEX ix_explorer_run_probe ON explorer_run(probe_id);
CREATE INDEX ix_explorer_run_settings ON explorer_run(explorer_settings_id);
CREATE INDEX ix_explorer_run_status ON explorer_run(status);
