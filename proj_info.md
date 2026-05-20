Project Proposal: AutoHeal
==========================

**Self-Healing Process Manager & Anomaly Detector**


Project Overview
----------------

Modern operating systems can occasionally fall victim to rogue applications—processes that enter infinite loops, suffer from memory leaks, or hoard CPU cycles, ultimately leading to system unresponsiveness. **AutoHeal** is a robust, low-level C++ background daemon designed to prevent these system lockups. It continuously monitors the Operating System's active processes, detects resource anomalies in real-time, and autonomously issues OS-level signals to recover system health without requiring manual user intervention. By bridging low-level systems programming with a modern web-based monitoring interface, AutoHeal acts as an automated safety net for the Linux environment.

Methodology & Architecture
--------------------------

The system is divided into five distinct, highly cohesive modules that handle everything from low-level kernel data extraction to high-level data visualization:

### 1\. The Observer (Data Ingestion)

The Observer acts as the eyes of the system. Instead of relying on high-level system monitoring tools, it interfaces directly with the Linux kernel via the virtual /proc filesystem.

*   **Mechanics:** It iteratively parses files like /proc/\[pid\]/stat and /proc/\[pid\]/status to extract raw, real-time metrics (e.g., CPU clock ticks, memory page usage, and process states).
    
*   **Efficiency:** Because /proc is highly dynamic, the Observer is optimized to perform lightweight file I/O operations, ensuring the monitoring tool itself does not become a resource burden.
    

### 2\. The Engine (Core Daemon)

The Engine is the central nervous system of AutoHeal, running entirely in the background as a Linux daemon.

*   **Daemonization:** It detaches from the controlling terminal using the standard double fork() and setsid() sequence, ensuring it runs persistently as a background service.
    
*   **Concurrency:** Built using POSIX Threads (pthreads), the Engine is heavily multi-threaded. Separate threads are dedicated to data collection, anomaly evaluation, and inter-process communication (IPC) to ensure the daemon remains responsive even under heavy system load.
    

### 3\. The Brain (Anomaly Detection)

This module processes the raw telemetry gathered by the Observer to determine if a process has gone rogue.

*   **Heuristics:** It employs lightweight threshold algorithms and tracks historical resource usage over time (moving averages).
    
*   **Signatures:** It looks for specific behavioral signatures, such as a process consuming 99% CPU for a sustained duration (indicating a potential infinite loop) or a continuous, unyielding increase in RAM usage (indicating a memory leak).
    

### 4\. The Healer (Action & Recovery)

When the Brain flags a rogue process, the Healer intervenes autonomously using Linux system calls and POSIX signals.

*   **Escalation Protocol:** To prevent data loss, the Healer uses a tiered recovery strategy. It first issues a SIGSTOP to temporarily freeze the process and halt resource consumption. If the process remains unstable upon continuation, it issues a SIGTERM for a graceful exit.
    
*   **Failsafe:** If the process ignores the graceful termination request (e.g., hanging in uninterruptible sleep), the Healer escalates to SIGKILL as a last resort to forcibly reclaim system resources.
    

### 5\. The Interface (Visualization & IPC)

While the daemon operates invisibly, system administrators need visibility into its actions.

*   **Communication:** A dedicated thread opens a WebSocket server in C++, establishing a bi-directional, low-latency IPC bridge.
    
*   **Dashboard:** A Next.js (React) web application connects to this socket, translating the serialized backend telemetry into a live, interactive dashboard that displays system health, active threats, and a log of autonomous interventions.
    

Security & Sanitization
-----------------------

Because AutoHeal interacts with critical OS components and executes process-terminating commands, security is heavily prioritized:

*   **Input Sanitization & Stability:** The /proc directory is volatile; processes can terminate exactly as AutoHeal attempts to read them. The system implements strict bounds-checking and try-catch mechanisms to prevent buffer overflows, segmentation faults, or parsing crashes. Furthermore, root system processes (like PID 1 or kernel threads) are hardcoded into an "ignore list" to prevent accidental system termination.
    
*   **Output Sanitization:** All telemetry data is strictly serialized into structured JSON before being transmitted over WebSockets. String outputs (like process names) are stripped of executable characters and sanitized to prevent log injection or Cross-Site Scripting (XSS) attacks on the Next.js frontend.
    

Tools and Technologies
----------------------

*   **Core Systems Programming:** C++, Standard Template Library (STL)
    
*   **OS APIs & Concurrency:** POSIX Threads (pthreads), Linux Signals (SIGSTOP, SIGTERM, SIGKILL), Process Control (fork(), exec())
    
*   **Kernel Interfacing:** Linux /proc virtual filesystem
    
*   **Networking / IPC:** C++ WebSocket libraries for inter-process communication
    
*   **Frontend Interface:** Next.js, React, TypeScript
    
*   **Development Environment:** Linux (Pop!\_OS / Ubuntu), GCC compiler, Makefile orchestration
    

Scope and Work Distribution
---------------------------

This project is highly suitable for a four-person team because it naturally divides into four distinct, substantial layers of computer science:

1.  **Low-Level Systems (Member 1):** Managing the /proc filesystem parsing, data extraction, and kernel-level metrics.
    
2.  **Concurrency & Architecture (Member 2):** Handling daemonization, thread management, mutex locks, and ensuring thread-safe operations.
    
3.  **Logic & Control (Member 3):** Designing the anomaly detection algorithms and managing the POSIX signal escalation paths.
    
4.  **Full-Stack Integration (Member 4):** Developing the C++ WebSocket server, creating the JSON serialization pipeline, and building the Next.js visual dashboard.
    

This multi-layered approach ensures that every group member tackles complex, OS-level challenges while collectively building a cohesive, full-stack product.