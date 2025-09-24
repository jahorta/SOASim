
1. work on reseting workers and resending jobs for stalled workers
    - see if return result - failure -> resend works for this.
1. try to remove frame limiter and test for consistency
1. work on the job storage database:
    - Optional reader pool: route OpType::Read to a small read thread pool with separate connections; keep single writer for OpType::Write
    - Micro-batching for writes: a short (e.g., 2–5 ms) coalescing window in the writer to group multiple pending write tasks into one transaction when safe.
    - Admin ops: Checkpoint(), Vacuum(), IntegrityCheck() as OpType::Admin.
    - Idempotency on hot ingest paths (e.g., predicate results keyed by (run_id, path_id, pred_id, turn)).