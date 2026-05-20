"use client";

import { useMemo, useState } from "react";
import type { ProcessRow } from "@/lib/types";

type SortKey = "pid" | "comm" | "cpu" | "rss_kb" | "state";

function fmtMB(kb: number) {
  return (kb / 1024).toFixed(1);
}

export function ProcessTable({
  rows,
  flaggedPids,
}: {
  rows: ProcessRow[];
  flaggedPids: Set<number>;
}) {
  const [sortKey, setSortKey] = useState<SortKey>("cpu");
  const [desc, setDesc] = useState(true);
  const [filter, setFilter] = useState("");

  const sorted = useMemo(() => {
    const f = filter.trim().toLowerCase();
    let r = rows;
    if (f) r = r.filter((p) => p.comm.toLowerCase().includes(f) || String(p.pid).includes(f));
    const arr = [...r];
    arr.sort((a, b) => {
      const av = a[sortKey] as number | string;
      const bv = b[sortKey] as number | string;
      if (av < bv) return desc ? 1 : -1;
      if (av > bv) return desc ? -1 : 1;
      return 0;
    });
    return arr.slice(0, 100);
  }, [rows, sortKey, desc, filter]);

  const onSort = (k: SortKey) => {
    if (k === sortKey) setDesc(!desc);
    else {
      setSortKey(k);
      setDesc(true);
    }
  };

  const Th = ({ k, label }: { k: SortKey; label: string }) => (
    <th
      onClick={() => onSort(k)}
      className="cursor-pointer text-left px-3 py-2 font-medium text-muted hover:text-slate-200 select-none"
    >
      {label}
      {sortKey === k && <span className="ml-1 text-accent">{desc ? "▼" : "▲"}</span>}
    </th>
  );

  return (
    <section className="bg-panel rounded-lg border border-panelAlt overflow-hidden">
      <div className="flex items-center justify-between px-4 py-3 border-b border-panelAlt">
        <h2 className="text-lg">Live Processes</h2>
        <input
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          placeholder="filter by name / pid"
          className="bg-bg border border-panelAlt rounded px-2 py-1 text-sm w-56 focus:outline-none focus:border-accent"
        />
      </div>
      <div className="overflow-auto max-h-[60vh]">
        <table className="w-full text-sm">
          <thead className="sticky top-0 bg-panel">
            <tr className="border-b border-panelAlt">
              <Th k="pid" label="PID" />
              <Th k="comm" label="Process" />
              <Th k="state" label="State" />
              <Th k="cpu" label="CPU %" />
              <Th k="rss_kb" label="RSS (MB)" />
              <th className="px-3 py-2 font-medium text-muted text-left">UID</th>
            </tr>
          </thead>
          <tbody>
            {sorted.map((p) => {
              const flagged = flaggedPids.has(p.pid);
              return (
                <tr
                  key={p.pid}
                  className={
                    "border-b border-panelAlt/60 " +
                    (flagged ? "bg-danger/15 text-danger" : "hover:bg-panelAlt/40")
                  }
                >
                  <td className="px-3 py-1.5 font-mono">{p.pid}</td>
                  <td className="px-3 py-1.5 truncate max-w-[20rem]">{p.comm}</td>
                  <td className="px-3 py-1.5 font-mono">{p.state}</td>
                  <td className="px-3 py-1.5 font-mono">
                    {p.cpu >= 90 ? <span className="text-danger">{p.cpu.toFixed(1)}</span> : p.cpu.toFixed(1)}
                  </td>
                  <td className="px-3 py-1.5 font-mono">{fmtMB(p.rss_kb)}</td>
                  <td className="px-3 py-1.5 font-mono">{p.uid}</td>
                </tr>
              );
            })}
            {sorted.length === 0 && (
              <tr>
                <td colSpan={6} className="text-center py-6 text-muted">
                  No processes (yet) — start the daemon to populate.
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </section>
  );
}
