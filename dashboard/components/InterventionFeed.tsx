"use client";

import type { InterventionRow } from "@/lib/types";

function stageColor(stage: string) {
  switch (stage) {
    case "OBSERVED":   return "bg-warning/20 text-warning";
    case "STOPPED":    return "bg-accent/20 text-accent";
    case "TERMINATED": return "bg-warning/30 text-warning";
    case "KILLED":     return "bg-danger/25 text-danger";
    case "RESOLVED":   return "bg-ok/20 text-ok";
    default:           return "bg-panelAlt text-muted";
  }
}

function reasonColor(reason: string) {
  switch (reason) {
    case "CPU_SPIN": return "text-danger";
    case "MEM_LEAK": return "text-warning";
    case "HUNG":     return "text-accent";
    default:         return "text-muted";
  }
}

function fmtTime(ms: number) {
  if (!ms) return "—";
  const d = new Date(ms);
  return d.toLocaleTimeString();
}

export function InterventionFeed({ rows }: { rows: InterventionRow[] }) {
  const sorted = [...rows].sort((a, b) => b.at_ms - a.at_ms).slice(0, 50);

  return (
    <section className="bg-panel rounded-lg border border-panelAlt overflow-hidden">
      <div className="px-4 py-3 border-b border-panelAlt flex items-center justify-between">
        <h2 className="text-lg">Intervention Log</h2>
        <span className="text-muted text-xs">latest 50 events</span>
      </div>
      <div className="overflow-auto max-h-[60vh]">
        {sorted.length === 0 ? (
          <div className="text-center py-10 text-muted text-sm">
            No interventions yet. Launch a rogue program (e.g. <code>./bin/rogue_cpu</code>) to see action.
          </div>
        ) : (
          <ul className="divide-y divide-panelAlt/60">
            {sorted.map((iv, i) => (
              <li key={`${iv.pid}-${iv.at_ms}-${i}`} className="px-4 py-3 text-sm">
                <div className="flex items-center justify-between mb-1">
                  <div className="flex items-center gap-3">
                    <span className="font-mono text-muted">{fmtTime(iv.at_ms)}</span>
                    <span className="font-mono">pid {iv.pid}</span>
                    <span className="text-slate-200 truncate max-w-[12rem]">{iv.comm}</span>
                  </div>
                  <span className={`px-2 py-0.5 rounded text-xs font-medium ${stageColor(iv.stage)}`}>
                    {iv.stage}
                  </span>
                </div>
                <div className="text-muted text-xs flex gap-4">
                  <span>reason: <span className={reasonColor(iv.reason)}>{iv.reason}</span></span>
                  {iv.signal > 0 && <span>signal: <span className="font-mono">{iv.signal}</span></span>}
                  <span className="truncate">{iv.outcome}</span>
                </div>
              </li>
            ))}
          </ul>
        )}
      </div>
    </section>
  );
}
