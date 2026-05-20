"use client";

import type { ConnState } from "@/lib/useAutoHealSocket";

export function StatusHeader({
  state,
  procCount,
  hostPid,
}: {
  state: ConnState;
  procCount: number;
  hostPid: number;
}) {
  const dot =
    state === "open" ? "bg-ok" : state === "connecting" ? "bg-warning" : "bg-danger";
  const label =
    state === "open" ? "Connected" : state === "connecting" ? "Connecting…" : "Disconnected";

  return (
    <header className="flex items-center justify-between px-6 py-4 border-b border-panelAlt bg-panel">
      <div className="flex items-center gap-3">
        <span className="text-accent text-2xl font-semibold tracking-tight">AutoHeal</span>
        <span className="text-muted text-sm">Self-Healing Process Manager</span>
      </div>
      <div className="flex items-center gap-6 text-sm">
        <span className="text-muted">
          host pid <span className="text-slate-200 font-mono">{hostPid || "—"}</span>
        </span>
        <span className="text-muted">
          processes <span className="text-slate-200 font-mono">{procCount}</span>
        </span>
        <span className="flex items-center gap-2">
          <span className={`h-2.5 w-2.5 rounded-full ${dot}`} />
          <span>{label}</span>
        </span>
      </div>
    </header>
  );
}
