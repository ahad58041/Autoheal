"use client";

import { useMemo } from "react";
import { useAutoHealSocket } from "@/lib/useAutoHealSocket";
import { StatusHeader } from "@/components/StatusHeader";
import { ProcessTable } from "@/components/ProcessTable";
import { InterventionFeed } from "@/components/InterventionFeed";
import { AnomalyBanner } from "@/components/AnomalyBanner";

export default function Home() {
  const { payload, state } = useAutoHealSocket();

  const flaggedPids = useMemo(() => {
    const s = new Set<number>();
    payload?.interventions.forEach((i) => {
      if (i.stage !== "RESOLVED") s.add(i.pid);
    });
    return s;
  }, [payload]);

  return (
    <main className="min-h-screen flex flex-col">
      <StatusHeader
        state={state}
        procCount={payload?.processes.length ?? 0}
        hostPid={payload?.host_pid ?? 0}
      />

      <div className="flex-1 px-6 py-6 space-y-6 max-w-[1400px] mx-auto w-full">
        <AnomalyBanner interventions={payload?.interventions ?? []} />

        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          <ProcessTable rows={payload?.processes ?? []} flaggedPids={flaggedPids} />
          <InterventionFeed rows={payload?.interventions ?? []} />
        </div>

        <footer className="text-center text-muted text-xs pt-4">
          AutoHeal · OS final project · live data over <code>ws://localhost:8080</code>
        </footer>
      </div>
    </main>
  );
}
