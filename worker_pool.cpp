#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    template<class F>
    void enqueue(F&& f);

    void waitUntilDone();

private:
    std::vector<std::thread> workers;
    std::queue<std::unique_ptr<std::function<void()>>> tasks; // Sử dụng unique_ptr cho các tác vụ

    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<size_t> activeTasks;

    void worker();
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false), activeTasks(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool() {
    stop = true;
    condition.notify_all();
    for (std::thread& worker : workers) {
        worker.join();
    }
}

template<class F>
void ThreadPool::enqueue(F&& f) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        // Tạo một unique_ptr cho tác vụ và emplace vào hàng đợi
        tasks.emplace(std::make_unique<std::function<void()>>(
            [this, task = std::forward<F>(f)]() {
                ++activeTasks; // Tăng đếm tác vụ đang hoạt động
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "Task encountered an error: " << e.what() << '\n';
                }
                --activeTasks; // Giảm đếm tác vụ đang hoạt động
            }
        ));
    }
    condition.notify_one();
}

void ThreadPool::worker() {
    while (true) {
        std::unique_ptr<std::function<void()>> task;

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });

            if (stop && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        // Gọi tác vụ thông qua unique_ptr
        (*task)();
    }
}

void ThreadPool::waitUntilDone() {
    while (activeTasks > 0) {
        std::this_thread::yield(); // Cho phép thread khác hoạt động
    }
}

// Example usage
int main() {
    ThreadPool pool(4);

    // Enqueue some tasks
    for (int i = 0; i < 10; ++i) {
        pool.enqueue([i] {
            std::cout << "Task " << i << " is being processed\n";
        });
    }

    pool.waitUntilDone(); // Đợi cho tất cả các tác vụ hoàn thành

    return 0;
}
