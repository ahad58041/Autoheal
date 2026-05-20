// Mirror of the backend JsonSerializer output. Keep in sync with
// src/interface/json_serializer.cpp.

export type ProcessRow = {
  pid: number;
  ppid: number;
  comm: string;
  state: string;
  cpu: number;       // percent of one logical CPU, already normalized
  rss_kb: number;
  vsize_kb: number;
  uid: number;
};

export type InterventionRow = {
  at_ms: number;
  pid: number;
  comm: string;
  reason: "CPU_SPIN" | "MEM_LEAK" | "HUNG" | string;
  stage: "OBSERVED" | "STOPPED" | "TERMINATED" | "KILLED" | "RESOLVED" | string;
  signal: number;
  outcome: string;
};

export type AutoHealPayload = {
  ts: number;
  ws_port: number;
  host_pid: number;
  processes: ProcessRow[];
  interventions: InterventionRow[];
};
