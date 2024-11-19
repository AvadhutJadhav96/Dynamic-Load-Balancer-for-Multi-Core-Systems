# Multi-Core Process Scheduler with Dynamic Load Balancing

## Overview

This project implements a multi-core process scheduling simulation with dynamic load balancing. The scheduler distributes incoming processes across available cores and balances the workload dynamically based on the load of each core.

The scheduler uses **Round-Robin** to assign incoming processes and **dynamic load balancing** to migrate processes between cores to maintain balanced loads.

### Key Features:
1. **Multi-Core Simulation**:
   - Each core runs as a separate thread.
   - Simulates process execution with `TIME_QUANTUM`.

2. **Dynamic Load Balancing**:
   - Balances the load among cores based on their respective workloads.
   - Uses a configurable threshold to decide task migration.

3. **Process Scheduling**:
   - Processes are assigned to cores based on Round-Robin scheduling.
   - Simulates process execution with support for preemption and re-queuing.

4. **Thread-Safe Design**:
   - Ensures thread safety using mutexes for critical sections.
   - Supports concurrent operations on process queues.

---

## How It Works

### Scheduler Workflow:
1. **Process Assignment**:
   - Incoming processes are assigned to cores in a Round-Robin manner.
   - Each core maintains its own queue of processes.

2. **Core Execution**:
   - Each core executes its assigned processes for a `TIME_QUANTUM`.
   - If a process isn’t completed, it is re-queued for further execution.

3. **Load Balancing**:
   - A background thread monitors the load across cores periodically.
   - If the load difference between the most loaded and least loaded core exceeds a threshold, a process is migrated.

4. **Process Simulation**:
   - Simulates process execution using `std::this_thread::sleep_for` for the required execution time.
   - Handles process arrival, execution, and completion events.
   - 

---

## Code Structure

### Key Classes:
1. **`Process`**:
   Represents a single process with attributes like `id`, `arrival_time`, `load`, `priority`, and `remaining_time`.

2. **`Core`**:
   Simulates a single CPU core. Each core:
   - Runs as a separate thread.
   - Maintains a queue of assigned processes.
   - Executes processes and reports completion.

3. **`Scheduler`**:
   Manages multiple cores and distributes processes.
   - Assigns processes to cores in Round-Robin fashion.
   - Dynamically balances load by migrating processes between cores.


