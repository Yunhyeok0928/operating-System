#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>

using namespace std;

struct Process {
    int id;
    bool isForeground;
    string command;
    int duration;
    int period;
    bool promoted = false;
};

class Scheduler {
private:
    queue<Process> dynamicQueue;
    queue<Process> waitQueue;
    mutex mtx;
    condition_variable cv;
    bool stop = false;
    int nextId = 0;

public:
    void addProcess(const Process& process) {
        lock_guard<mutex> lock(mtx);
        dynamicQueue.push(process);
        cv.notify_one();
    }

    void promoteProcess() {
        lock_guard<mutex> lock(mtx);
        if (!dynamicQueue.empty()) {
            Process p = dynamicQueue.front();
            dynamicQueue.pop();
            p.promoted = true;
            dynamicQueue.push(p);
        }
    }

    void printState() {
        lock_guard<mutex> lock(mtx);
        cout << "Running: [1B]" << endl;
        cout << "---------------------------" << endl;
        cout << "DQ: ";
        queue<Process> tempQueue = dynamicQueue;
        while (!tempQueue.empty()) {
            Process p = tempQueue.front();
            tempQueue.pop();
            cout << (p.isForeground ? to_string(p.id) + "F" : to_string(p.id) + "B");
            if (p.promoted) {
                cout << "*";
            }
            if (!tempQueue.empty()) {
                cout << " ";
            }
        }
        cout << endl;
        cout << "---------------------------" << endl;
        cout << "WQ: ";
        tempQueue = waitQueue;
        while (!tempQueue.empty()) {
            Process p = tempQueue.front();
            tempQueue.pop();
            cout << to_string(p.id) << (p.isForeground ? "F:" : "B:") << p.duration;
            if (!tempQueue.empty()) {
                cout << " ";
            }
        }
        cout << endl;
        cout << "---------------------------" << endl;
    }

    void schedule() {
        while (!stop) {
            unique_lock<mutex> lock(mtx);
            cv.wait_for(lock, chrono::seconds(1), [this]() { return !dynamicQueue.empty() || stop; });

            if (!dynamicQueue.empty()) {
                Process currentProcess = dynamicQueue.front();
                dynamicQueue.pop();
                lock.unlock();

                vector<string> args = parseCommand(currentProcess.command);
                executeCommand(args);

                lock.lock();
                currentProcess.duration -= 1;
                if (currentProcess.duration > 0) {
                    waitQueue.push(currentProcess);
                }
            }
        }
    }

    void stopScheduler() {
        stop = true;
        cv.notify_all();
    }

    vector<string> parseCommand(const string& command) {
        vector<string> tokens;
        stringstream ss(command);
        string token;
        while (getline(ss, token, ' ')) {
            tokens.push_back(token);
        }
        return tokens;
    }

    void executeCommand(const vector<string>& args) {
        if (args.empty()) return;

        if (args[0] == "echo") {
            for (size_t i = 1; i < args.size(); ++i) {
                cout << args[i] << " ";
            }
            cout << endl;
        }
        else if (args[0] == "dummy") {
            // Do nothing
        }
        else if (args[0] == "gcd") {
            if (args.size() >= 3) {
                int a = stoi(args[1]);
                int b = stoi(args[2]);
                cout << "GCD of " << a << " and " << b << " is " << gcd(a, b) << endl;
            }
        }
        else if (args[0] == "prime") {
            if (args.size() >= 2) {
                int x = stoi(args[1]);
                cout << "Number of primes less than or equal to " << x << " is " << countPrimes(x) << endl;
            }
        }
        else if (args[0] == "sum") {
            if (args.size() >= 2) {
                int x = stoi(args[1]);
                cout << "Sum modulo 1,000,000 of integers up to " << x << " is " << sumModulo(x) << endl;
            }
        }
    }

    int gcd(int a, int b) {
        while (b != 0) {
            int t = b;
            b = a % b;
            a = t;
        }
        return a;
    }

    int countPrimes(int n) {
        vector<bool> isPrime(n + 1, true);
        isPrime[0] = isPrime[1] = false;
        for (int i = 2; i * i <= n; ++i) {
            if (isPrime[i]) {
                for (int j = i * i; j <= n; j += i) {
                    isPrime[j] = false;
                }
            }
        }
        return count(isPrime.begin(), isPrime.end(), true);
    }

    int sumModulo(int n) {
        long long sum = 0;
        for (int i = 1; i <= n; ++i) {
            sum = (sum + i) % 1000000;
        }
        return static_cast<int>(sum);
    }
};

void CLI(Scheduler& scheduler) {
    ifstream file("commands.txt");
    string line;
    int id = 0;
    while (getline(file, line)) {
        vector<string> commands = scheduler.parseCommand(line);
        for (const string& cmd : commands) {
            Process process = { id++, cmd.find('&') != string::npos ? false : true, cmd, 5, 0 };
            scheduler.addProcess(process);
            this_thread::sleep_for(chrono::seconds(5)); // Simulate user input interval
        }
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
