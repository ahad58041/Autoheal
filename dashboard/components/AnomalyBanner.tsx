"use client";

import type { InterventionRow } from "@/lib/types";

export function AnomalyBanner({ interventions }: { interventions: InterventionRow[] }) {
  // Active = any row whose stage isn't RESOLVED, within the last 30 seconds.
  const now = Date.now();
  const active = interventions.filter(
    (i) => i.stage !== "RESOLVED" && now - i.at_ms < 30_000,
  );
  if (active.length === 0) {
    return (
      <div className="px-4 py-3 rounded-lg border border-ok/30 bg-ok/5 text-ok text-sm">
        ✓ System healthy — no active anomalies.
      </div>
    );
  }
  return (
    <div className="px-4 py-3 rounded-lg border border-danger/40 bg-danger/10 text-danger text-sm">
      ⚠ {active.length} active anomal{active.length === 1 ? "y" : "ies"}: {" "}
      {active
        .slice(0, 3)
        .map((i) => `${i.comm}(${i.pid}) ${i.reason}`)
        .join(" • ")}
      {active.length > 3 && <span className="text-muted"> + {active.length - 3} more</span>}
    </div>
  );
}
