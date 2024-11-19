#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>
using namespace std;


const int LOW_LOAD = 1;
const int MEDIUM_LOAD = 2;
const int HIGH_LOAD = 3;
const int TIME_QUANTUM = 15; 
struct Process {
    int id;
    int arrival_time;
    int total_execution_time;
    int remaining_time;
    int load;
    int priority;
    Process(int pid, int arrival, int execution_time, int lo, int pri = 0){
        id = pid; arrival_time = arrival; total_execution_time = execution_time; load = lo; priority = pri;
        remaining_time = execution_time;
    }
    Process(){}
};


struct ComparePriority {
    bool operator()(const Process& a, const Process& b) {
        return a.priority < b.priority;
    }
};

class Core {
public:
    
    int id;
    queue<Process> process_queue;
    mutex mtx;
    int current_load; 
    thread core_thread; 
    bool stop;
    Core(int core_id)
        : id(core_id), current_load(0), stop(false){}
    void run(){
        while(!stop) {
            Process current_process;
            bool has_process = false;
            
            
            {
                lock_guard<mutex> lock(mtx);
                if(!process_queue.empty()) {
                    current_process = process_queue.front();
                    process_queue.pop();
                    current_load -= current_process.load;
                    has_process = true;
                }
            }
            if(has_process) {

                int exec_time = std::min(TIME_QUANTUM, current_process.remaining_time);
                std::cout << "Core " << id << " executing Process P" << current_process.id 
                        << " for " << exec_time << "ms\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(exec_time));
                current_process.remaining_time -= exec_time;
                if(current_process.remaining_time > 0) {
                    {

                        lock_guard<mutex> lock(mtx);
                        process_queue.push(current_process);
                        current_load += current_process.load;
                    }
                } else {
                    cout << "Process P" << current_process.id << " completed on Core " << id << "\n";
                }
            } else {
        
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    void start(); 

    int getCoreLoad(){
        
        return current_load;
    }


    void stop_core() {
        stop = true;
       
    }
};


class Scheduler {
private:
    
public:
    vector<Core*> cores;
   
   
    mutex ready_queue_mtx;
    int which_core_turn = 0; 
    thread balancer_thread;
    bool stop = 0;
    int num_cores = 0;
    Scheduler(int cores_count){
        stop = false;
        num_cores = cores_count;
        for(int i = 0; i < cores_count; ++i) {
            cores.push_back(new Core(i));
        }
    }
    ~Scheduler() {

        for(auto core : cores) {
            core->stop_core();
            if(core->core_thread.joinable())
                core->core_thread.join();
            delete core;
        }

        stop = true;
        if(balancer_thread.joinable())
            balancer_thread.join();
    }

    void assign_process_to_core(const Process& proc) {
        Core* core = cores[which_core_turn];
        {
            lock_guard<mutex> lock(core->mtx);
            core->process_queue.push(proc);
            core->current_load += proc.load;
            lock_guard<mutex> lock1(ready_queue_mtx);
            which_core_turn = (which_core_turn + 1)%num_cores;
        }
        cout << "Assigned Process P" << proc.id << " to Core " << core->id << "\n";
    }

    void start() {

        for(auto core : cores) {
            core->core_thread = std::thread(&Core::run, core);
        }

        balancer_thread = std::thread(&Scheduler::balance_load, this);
    }
    
    
    void balance_load() {
        while(!stop) {
            this_thread::sleep_for(chrono::milliseconds(50)); // Balance every 50ms
            /* For calculating load on a core with respect to other cores
             The sum of the time-slice sizes (LOW,MEDIUM,HIGH) of all ready tasks on a single processor is regarded 
             as the processor load which is denoted by L. And T denotes the load proportion threshold. 
             In our design, the value of T is 20%. 'A' denotes the average processor load of all CPUs in 
             the current system. The value of L/A is referred 
             to as the degree of load on the core
             heavy : L > A*(1 + T)
             light : L < A 
            */
            // Calculate average load
            
            int total_load = 0;
            for(auto core : cores) {

                total_load += core->getCoreLoad();
            }
            double average_load = static_cast<double>(total_load) / cores.size();
            double threshold = average_load * 0.2; // 20% threshold

            Core* max_core = nullptr;
            Core* min_core = nullptr;
            int max_load = INT32_MIN;
            int min_load = INT32_MAX;
            for(auto core : cores) {

                int load = core->current_load;
                if(load > max_load) {
                    max_load = load;
                    max_core = core;
                }
                if(load < min_load) {
                    min_load = load;
                    min_core = core;
                }
            }
            if(max_core && min_core && (max_load - min_load > threshold)) {
                
                std::lock(max_core->mtx, min_core->mtx);
                std::lock_guard<std::mutex> lock1(max_core->mtx, std::adopt_lock);
                std::lock_guard<std::mutex> lock2(min_core->mtx, std::adopt_lock);
                if(!max_core->process_queue.empty()) {
                    Process proc = max_core->process_queue.front();
                    max_core->process_queue.pop();
                    max_core->current_load -= proc.load;
                 
                 
                    if(proc.remaining_time > 0) {
                        min_core->process_queue.push(proc);
                        min_core->current_load += proc.load;
                        std::cout << "Balancer: Moved Process P" << proc.id 
                                  << " from Core " << max_core->id 
                                  << " to Core " << min_core->id << std::endl;
                    }
                }
            }
        }
    }
    void taskMigration(Core* max_core, Core* min_core){
        lock(max_core->mtx, min_core->mtx);
        lock_guard<mutex> lock1(max_core->mtx,adopt_lock);
        lock_guard<mutex> lock2(min_core->mtx,adopt_lock);
        if (!max_core->process_queue.empty())
        {
            Process proc = max_core->process_queue.front();
            max_core->process_queue.pop();
            max_core->current_load -= proc.load;
           
            if (proc.remaining_time > 0)
            {
                min_core->process_queue.push(proc);
                min_core->current_load += proc.load;
                std::cout << "Balancer: Moved Process P" << proc.id
                          << " from Core " << max_core->id
                          << " to Core " << min_core->id << std::endl;
            }
        }
    }
    
    
    void display_assignment() {
        std::cout << "\nProcess Assignment to Cores:\n";
        for(auto core : cores) {
            std::lock_guard<std::mutex> lock(core->mtx);
            std::queue<Process> temp = core->process_queue;
            std::cout << "Core " << core->id << " [Load: " << core->current_load << "]: ";
            while(!temp.empty()) {
                std::cout << "P" << temp.front().id << "(" 
                          << (temp.front().load == LOW_LOAD ? "Low" : "High") 
                          << ", Rem: " << temp.front().remaining_time << "ms) ";
                temp.pop();
            }
            std::cout << "\n";
        }
    }
};

bool cmp(Process a , Process b)
{
    return a.arrival_time < b.arrival_time;
}

int main(){
    int num_cores = 2;
    Scheduler scheduler(num_cores);

    std::vector<Process> processes = {
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
    
    sort(processes.begin(),processes.end(),cmp);
    
    scheduler.start();
    auto start_time = std::chrono::steady_clock::now();
    for(auto& proc : processes) {
        // Wait until the process's arrival time
        auto now = std::chrono::steady_clock::now(); // its in nanosec in general
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if(proc.arrival_time > elapsed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(proc.arrival_time - elapsed));
        }
        std::cout << "Process P" << proc.id << " arrived (Load: " 
                      << (proc.load == LOW_LOAD ? "Low" : "High") 
                      << ", Priority: " << proc.priority << ")\n";
            scheduler.assign_process_to_core(proc);
    }
    // Let the simulation run for a certain period
    std::this_thread::sleep_for(std::chrono::seconds(5));
    // Stop the scheduler
    scheduler.stop = true;
    // Allow some time for cores to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Display final assignment
    scheduler.display_assignment();
    return 0;
}
