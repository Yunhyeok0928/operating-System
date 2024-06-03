#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <fstream>
#include <string>

using namespace std;

struct Process {
    int id;
    bool isForeground;
    string command;
};

class Scheduler {
private:
    queue<Process> dynamicQueue;
    queue<Process> waitQueue;
    mutex mtx;
    condition_variable cv;
    bool stop = false;

public:
    void addProcess(const Process& process) {
        lock_guard<mutex> lock(mtx);
        dynamicQueue.push(process);
        cv.notify_one();
    }

    void schedule() {
        while (!stop) {
            unique_lock<mutex> lock(mtx);
            cv.wait_for(lock, chrono::seconds(1), [this]() { return !dynamicQueue.empty() || stop; });

            if (!dynamicQueue.empty()) {
                Process currentProcess = dynamicQueue.front();
                dynamicQueue.pop();
                lock.unlock();

                if (currentProcess.isForeground) {
                    cout << "Executing command: " << currentProcess.command << endl;
                    // Simulate command execution
                    this_thread::sleep_for(chrono::seconds(1));
                }
                else {
                    // Background process
                    cout << "Monitoring system state..." << endl;
                    // Simulate monitoring
                    this_thread::sleep_for(chrono::seconds(1));
                }

                lock.lock();
                waitQueue.push(currentProcess);
            }
        }
    }

    void stopScheduler() {
        stop = true;
        cv.notify_all();
    }

    void printState() {
        lock_guard<mutex> lock(mtx);
        cout << "Dynamic Queue size: " << dynamicQueue.size() << endl;
        cout << "Wait Queue size: " << waitQueue.size() << endl;
    }
};

void CLI(Scheduler& scheduler) {
    ifstream file("commands.txt");
    string line;
    int id = 0;
    while (getline(file, line)) {
        Process process = { id++, true, line };
        scheduler.addProcess(process);
        this_thread::sleep_for(chrono::seconds(5)); // Simulate user input interval
    }
}

void monitor(Scheduler& scheduler) {
    while (true) {
        this_thread::sleep_for(chrono::seconds(10)); // Simulate monitoring interval
        scheduler.printState();
    }
}

int main() {
    Scheduler scheduler;

    thread cliThread(CLI, ref(scheduler));
    thread monitorThread(monitor, ref(scheduler));
    thread schedulerThread(&Scheduler::schedule, &scheduler);

    cliThread.join();
    monitorThread.detach();
    scheduler.stopScheduler();
    schedulerThread.join();

    cout << "===end of main()===\n";
    return 0;
}
