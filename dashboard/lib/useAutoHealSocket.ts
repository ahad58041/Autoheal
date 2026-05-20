// React hook that maintains a WebSocket connection to the AutoHeal daemon
// and exposes the most recent payload + connection status.

"use client";

import { useEffect, useRef, useState } from "react";
import type { AutoHealPayload } from "./types";

export type ConnState = "connecting" | "open" | "closed";

const DEFAULT_URL =
  typeof window !== "undefined"
    ? `ws://${window.location.hostname || "localhost"}:8080`
    : "ws://localhost:8080";

export function useAutoHealSocket(url: string = DEFAULT_URL) {
  const [payload, setPayload] = useState<AutoHealPayload | null>(null);
  const [state, setState] = useState<ConnState>("connecting");
  const wsRef = useRef<WebSocket | null>(null);
  const retryRef = useRef<number>(0);

  useEffect(() => {
    let cancelled = false;

    const connect = () => {
      if (cancelled) return;
      setState("connecting");
      const ws = new WebSocket(url);
      wsRef.current = ws;

      ws.onopen = () => {
        retryRef.current = 0;
        setState("open");
      };
      ws.onmessage = (evt) => {
        try {
          const data = JSON.parse(evt.data) as AutoHealPayload;
          setPayload(data);
        } catch {
          // ignore malformed frames
        }
      };
      ws.onclose = () => {
        setState("closed");
        if (cancelled) return;
        // Exponential backoff, capped at 5s.
        const delay = Math.min(5000, 500 * Math.pow(2, retryRef.current));
        retryRef.current += 1;
        setTimeout(connect, delay);
      };
      ws.onerror = () => {
        ws.close();
      };
    };

    connect();
    return () => {
      cancelled = true;
      wsRef.current?.close();
    };
  }, [url]);

  return { payload, state };
}
