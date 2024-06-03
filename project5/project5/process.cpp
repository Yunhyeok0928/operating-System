// 2 - 3 second  커밋시점까지 진행했습니다.

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <list>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

struct Process {
    int id;
    bool isForeground;
    string command;
    int duration;
    int period;
    bool promoted = false;
    int remainingTime = 0;
};

class Scheduler {
private:
    list<list<Process>> stack;
    list<Process> waitQueue;
    mutex mtx;
    condition_variable cv;
    bool stop = false;
    int nextId = 0;
    size_t threshold = 5;

public:
    void enqueue(const Process& process) {
        lock_guard<mutex> lock(mtx);
        if (stack.empty() || (process.isForeground && stack.front().empty())) {
            stack.push_front(list<Process>());
        }
        if (process.isForeground) {
            stack.front().push_back(process);
        }
        else {
            if (stack.empty()) {
                stack.push_back(list<Process>());
            }
            stack.back().push_front(process);
        }
        cv.notify_one();
    }

    Process dequeue() {
        lock_guard<mutex> lock(mtx);
        if (stack.empty() || stack.front().empty()) {
            throw runtime_error("Queue is empty");
        }
        Process process = stack.front().front();
        stack.front().pop_front();
        if (stack.front().empty()) {
            stack.pop_front();
        }
        return process;
    }

    void promote() {
        lock_guard<mutex> lock(mtx);
        if (stack.empty() || stack.size() == 1) return;

        list<Process>& top = stack.front();
        list<Process>& bottom = stack.back();

        Process p = top.front();
        top.pop_front();
        bottom.push_back(p);

        if (top.empty()) {
            stack.pop_front();
        }
        if (bottom.size() > threshold) {
            split_n_merge();
        }
    }

    void split_n_merge() {
        lock_guard<mutex> lock(mtx);
        for (auto it = stack.begin(); it != stack.end(); ++it) {
            if (it->size() > threshold) {
                list<Process> new_list;
                auto mid = next(it->begin(), it->size() / 2);
                new_list.splice(new_list.begin(), *it, mid, it->end());
                stack.insert(next(it), new_list);
            }
        }
    }

    void printState() {
        lock_guard<mutex> lock(mtx);
        cout << "Running: [1B]" << endl;
        cout << "---------------------------" << endl;
        cout << "DQ: ";
        for (const auto& lst : stack) {
            for (const auto& p : lst) {
                cout << (p.isForeground ? to_string(p.id) + "F" : to_string(p.id) + "B");
                if (p.promoted) {
                    cout << "*";
                }
                cout << " ";
            }
        }
        cout << endl;
        cout << "---------------------------" << endl;
        cout << "WQ: ";
        for (const auto& p : waitQueue) {
            cout << to_string(p.id) << (p.isForeground ? "F:" : "B:") << p.remainingTime << " ";
        }
        cout << endl;
        cout << "---------------------------" << endl;
    }

    void schedule() {
        while (!stop) {
            unique_lock<mutex> lock(mtx);
            cv.wait_for(lock, chrono::seconds(1), [this]() { return !stack.empty() || stop; });

            if (!stack.empty()) {
                Process currentProcess = dequeue();
                lock.unlock();

                vector<string> args = parseCommand(currentProcess.command);
                executeCommand(args);

                lock.lock();
                currentProcess.duration -= 1;
                if (currentProcess.duration > 0) {
                    waitQueue.push_back(currentProcess);
                }
            }

            for (auto it = waitQueue.begin(); it != waitQueue.end();) {
                it->remainingTime -= 1;
                if (it->remainingTime <= 0) {
                    enqueue(*it);
                    it = waitQueue.erase(it);
                }
                else {
                    ++it;
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

void CLI(Scheduler& scheduler, int Y) {
    ifstream file("commands.txt");
    string line;
    int id = 0;
    while (getline(file, line)) {
        stringstream ss(line);
        string cmd;
        while (getline(ss, cmd, ';')) {
            vector<string> commands = scheduler.parseCommand(cmd);
            bool isForeground = true;
            if (!commands.empty() && commands[0][0] == '&') {
                isForeground = false;
                commands[0] = commands[0].substr(1);
            }
            Process process = { id++, isForeground, cmd, 5, 0 };
            scheduler.enqueue(process);
            this_thread::sleep_for(chrono::seconds(Y));
        }
    }
}

void monitor(Scheduler& scheduler, int X) {
    while (true) {
        this_thread::sleep_for(chrono::seconds(X));
        scheduler.printState();
    }
}

int main() {
    Scheduler scheduler;
    int Y = 5; // Shell 프로세스가 명령어를 실행한 후 대기하는 시간 (초)
    int X = 20; // Monitor 프로세스가 상태를 출력하는 간격 (초)

    thread cliThread(CLI, ref(scheduler), Y);
    thread monitorThread(monitor, ref(scheduler), X);
    thread schedulerThread(&Scheduler::schedule, &scheduler);

    cliThread.join();
    monitorThread.detach();
    scheduler.stopScheduler();
    schedulerThread.join();

    cout << "===end of main()===\n";
    return 0;
}
