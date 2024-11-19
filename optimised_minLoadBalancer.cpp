#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
#include <bits/stdc++.h>

using namespace std;

// Constants for load types
const int LOW_LOAD = 1;
const int MEDIUM_LOAD = 2;
const int HIGH_LOAD = 3;
const int TIME_QUANTUM = 15; // Time quantum in ms

// Structure to define a process
struct Process {
    int id, arrival_time, total_execution_time, remaining_time, load, priority;

    Process(int pid, int arrival, int execution_time, int lo, int pri = 0)
        : id(pid), arrival_time(arrival), total_execution_time(execution_time),
          load(lo), priority(pri), remaining_time(execution_time) {}

    Process() {}
};


// Core class simulates a processing unit
class Core {
public:
    int id;
    map<int, queue<Process>, greater<int>> process_queues;
    mutex mtx;
    int current_load;
    thread core_thread;
    bool stop;

    Core(int core_id) : id(core_id), current_load(0), stop(false) {}

    // Simulates core execution
    void run() {
        while (!stop) {
            Process current_process;
            bool has_process = false;

            {
                lock_guard<mutex> lock(mtx);
                for (auto& entry : process_queues) {
                    if (!entry.second.empty()) {
                        current_process = entry.second.front();
                        entry.second.pop();
                        current_load -= current_process.load;
                        has_process = true;
                        break;
                    }
                }
            }

            if (has_process) {
                int exec_time = min(TIME_QUANTUM, current_process.remaining_time);
                cout << "Core " << id << " executing Process P" << current_process.id
                     << " (Priority " << current_process.priority << ") for " << exec_time << "ms\n";
                this_thread::sleep_for(chrono::milliseconds(exec_time));
                current_process.remaining_time -= exec_time;

                if (current_process.remaining_time > 0) {
                    lock_guard<mutex> lock(mtx);
                    process_queues[current_process.priority].push(current_process);
                    current_load += current_process.load;
                } else {
                    cout << "Process P" << current_process.id << " completed on Core " << id << "\n";
                }
            } else {
                this_thread::sleep_for(chrono::milliseconds(10));
            }
        }
    }

    // Stops the core's execution
    void stop_core() { stop = true; }

    // Returns the current load on the core
    int getCoreLoad() { return current_load; }
};

// Scheduler class manages process assignment and load balancing
class Scheduler {
public:
    vector<Core*> cores;
    mutex ready_queue_mtx;
    int which_core_turn = 0;
    thread balancer_thread;
    bool stop = false;
    int num_cores = 0;

    Scheduler(int cores_count) : num_cores(cores_count) {
        for (int i = 0; i < cores_count; ++i) {
            cores.push_back(new Core(i));
        }
    }

    ~Scheduler() {
        for (auto core : cores) {
            core->stop_core();
            if (core->core_thread.joinable())
                core->core_thread.join();
            delete core;
        }
        stop = true;
        if (balancer_thread.joinable())
            balancer_thread.join();
    }

    // Assigns a process to a core
    void assign_process_to_core(const Process& proc) {
        Core* core = cores[which_core_turn];
        {
            lock_guard<mutex> lock(core->mtx);
            core->process_queues[proc.priority].push(proc);
            core->current_load += proc.load;

            lock_guard<mutex> lock1(ready_queue_mtx);
            which_core_turn = (which_core_turn + 1) % num_cores;
        }
        cout << "Assigned Process P" << proc.id << " to Core " << core->id << "\n";
    }

    // Starts the scheduler and cores
    void start() {
        for (auto core : cores) {
            core->core_thread = thread(&Core::run, core);
        }
        balancer_thread = thread(&Scheduler::balance_load, this);
    }

    // Dynamically balances the load across cores
    void balance_load() {
        while (!stop) {
            this_thread::sleep_for(chrono::milliseconds(50));
            int total_load = 0;

            for (auto core : cores)
                total_load += core->getCoreLoad();

            double average_load = static_cast<double>(total_load) / cores.size();
            double threshold = average_load * 0.2;

            Core *max_core = nullptr, *min_core = nullptr;
            int max_load = INT_MIN, min_load = INT_MAX;

            for (auto core : cores) {
                int load = core->current_load;
                if (load > max_load) max_core = core, max_load = load;
                if (load < min_load) min_core = core, min_load = load;
            }

            if (max_core && min_core && (max_load - min_load > threshold)) {
                lock(max_core->mtx, min_core->mtx);
                lock_guard<mutex> lock1(max_core->mtx, adopt_lock);
                lock_guard<mutex> lock2(min_core->mtx, adopt_lock);

                for (auto& entry : max_core->process_queues) {
                    if (!entry.second.empty()) {
                        Process proc = entry.second.front();
                        entry.second.pop();
                        max_core->current_load -= proc.load;
                        min_core->process_queues[proc.priority].push(proc);
                        min_core->current_load += proc.load;
                        cout << "Balancer: Moved Process P" << proc.id
                             << " from Core " << max_core->id
                             << " to Core " << min_core->id << "\n";
                        break;
                    }
                }
            }
        }
    }

    // Displays process assignment for each core
    void display_assignment() {
        cout << "\nProcess Assignment to Cores:\n";
        for (auto core : cores) {
            lock_guard<mutex> lock(core->mtx);
            cout << "Core " << core->id << " [Load: " << core->current_load << "]: ";
            for (const auto& entry : core->process_queues) {
                queue<Process> temp = entry.second;
                cout << "[Priority " << entry.first << "]: ";
                while (!temp.empty()) {
                    cout << "P" << temp.front().id << "(" << (temp.front().load == LOW_LOAD ? "Low" : "High")
                         << ", Rem: " << temp.front().remaining_time << "ms) ";
                    temp.pop();
                }
                cout << " ";
            }
            cout << "\n";
        }
    }
};

bool cmp(Process a , Process b)
{
    return a.arrival_time < b.arrival_time;
}

int main() {
    int num_cores = 2;
    Scheduler scheduler(num_cores);
    vector<Process> processes;

    processes = {
        {1, 0, 125, LOW_LOAD, 1},
        {2, 5, 150, HIGH_LOAD, 3},
        {3, 10, 150, LOW_LOAD, 2},
        {4, 15, 100, HIGH_LOAD, 1}
        // {5, 20, 180, LOW_LOAD, 2},
        // {6, 25, 220, HIGH_LOAD, 3},
        // {7, 30, 170, LOW_LOAD, 1},
        // {8, 35, 120, HIGH_LOAD, 2},
        // {9, 40, 140, LOW_LOAD, 3},
        // {10, 45, 160, HIGH_LOAD, 1}
    };

    // cout << "Enter the number of processes: ";
    // int num_process; cin >> num_process;

    // for (int i = 0; i < num_process; ++i) {
    //     Process p;
    //     p.id = i + 1;
    //     cout << "Process " << i + 1 << ":\n";
    //     cout<< "Enter the info for the Process " << i+1 << endl;
    //     cout << "Arrival time: "; cin >> p.arrival_time;
    //     cout << "Execution time: "; cin >> p.total_execution_time;
    //     p.remaining_time = p.total_execution_time;

    //     cout << "Load type (H for High, M for Medium, L for Low): ";
    //     char c; cin >> c;
    //     p.load = (c == 'H' ? HIGH_LOAD : (c == 'M' ? MEDIUM_LOAD : LOW_LOAD));

    //     cout << "Priority: "; cin >> p.priority;
    //     processes.push_back(p);
    // }

    sort(processes.begin(),processes.end(),cmp);

    scheduler.start();

    auto start_time = chrono::steady_clock::now();
    for (auto& proc : processes) {
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - start_time).count();
        if (proc.arrival_time > elapsed)
            this_thread::sleep_for(chrono::milliseconds(proc.arrival_time - elapsed));

        cout << "Process P" << proc.id << " arrived (Load: " << (proc.load == LOW_LOAD ? "Low" : "High")
             << ", Priority: " << proc.priority << ")\n";
        scheduler.assign_process_to_core(proc);
    }

    this_thread::sleep_for(chrono::seconds(5));
    scheduler.stop = true;
    this_thread::sleep_for(chrono::milliseconds(100));
    scheduler.display_assignment();

    return 0;
}
