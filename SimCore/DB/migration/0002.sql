-- Guard: record current schema level
CREATE TABLE IF NOT EXISTS schema_version (
  version    INTEGER NOT NULL,
  applied_at INTEGER NOT NULL
);
DELETE FROM schema_version;
INSERT INTO schema_version(version, applied_at)
VALUES (2, strftime('%s','now'));

-- Hot-path partial indexes (status filtering for queues/dashboards)
CREATE INDEX IF NOT EXISTS ix_seed_probe_active
  ON seed_probe(savestate_id, status)
  WHERE status IN ('planned','queued','running');

CREATE INDEX IF NOT EXISTS ix_seed_delta_active
  ON seed_delta(probe_id, status)
  WHERE status IN ('planned','queued','running');

CREATE INDEX IF NOT EXISTS ix_explorer_run_active
  ON explorer_run(probe_id, delta_id, explorer_settings_id)
  WHERE status IN ('planned','queued','running');

-- Covering-ish index for quick lineage filters
CREATE INDEX IF NOT EXISTS ix_explorer_run_lineage_status
  ON explorer_run(status, probe_id, delta_id, explorer_settings_id);

-- Convenience view: full lineage of a run (optional)
CREATE VIEW IF NOT EXISTS vw_explorer_run_lineage AS
SELECT
  er.id                AS explorer_run_id,
  er.status            AS run_status,
  er.progress_pct,
  er.started_at,
  er.finished_at,
  er.ui_config_hash,
  er.vm_program_hash,
  sd.id                AS seed_delta_id,
  sd.segment_kind,
  sd.seed_delta,
  sp.id                AS seed_probe_id,
  sp.neutral_seed,
  ss.id                AS savestate_id,
  ss.savestate_type,
  ss.sha256            AS savestate_sha256
FROM explorer_run er
JOIN seed_delta sd   ON sd.id = er.delta_id
JOIN seed_probe sp   ON sp.id = er.probe_id
JOIN savestate ss    ON ss.id = sp.savestate_id;
