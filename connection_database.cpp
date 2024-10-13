#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <memory>
#include <string>

class DatabaseConnection {
private:
    std::string connStr; // kết nối
    std::string queryResult; // Biến lưu trữ kết quả truy vấn

public:
    DatabaseConnection(const std::string& connStr)
        : connStr(connStr), queryResult("") {
        std::cout << "Kết nối tới cơ sở dữ liệu: Thực hiện logic tạo kết nối với đầu vào " << connStr << std::endl;
    }

    void execute(const std::string& query) {
        // Giả lập thực hiện truy vấn
        std::cout << "Executing query: " << query << std::endl;
        // Giả lập thời gian thực hiện
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Giả lập thời gian thực hiện

        // Cập nhật kết quả truy vấn
        queryResult = "Kết quả của truy vấn '" + query + "'"; // Cập nhật biến lưu trữ kết quả
    }

    std::string getResult() const {
        return queryResult; // Trả về kết quả truy vấn đã lưu
    }
};

// Lớp ConnectionPool để quản lý kết nối
class ConnectionPool {
private:
    std::vector<std::shared_ptr<DatabaseConnection>> connections;
    int maxConnections;
    std::mutex mutex;
    std::condition_variable condition;
    bool shutdown = false;  // Cờ để báo hiệu shutdown

public:
    ConnectionPool(const std::string& connStr, int maxSize)
        : maxConnections(maxSize) {
        for (int i = 0; i < maxConnections; ++i) {
            connections.push_back(std::make_shared<DatabaseConnection>(connStr));
        }
    }

    ~ConnectionPool() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            shutdown = true;
        }
        condition.notify_all();  // Thông báo cho tất cả các worker thread
    }

    std::shared_ptr<DatabaseConnection> getConnection() {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this]() { return shutdown || !connections.empty(); });

        if (shutdown) {
            return nullptr;  // Nếu đang shutdown, trả về nullptr
        }

        auto connection = connections.back();
        connections.pop_back();  // Lấy kết nối ra khỏi pool
        return connection;
    }

    void releaseConnection(std::shared_ptr<DatabaseConnection> connection) {
        std::lock_guard<std::mutex> lock(mutex);
        connections.push_back(connection);  // Trả kết nối về pool
        condition.notify_one();  // Thông báo rằng đã có kết nối trống
    }
};

// Cấu trúc lưu trữ kết quả
struct ResultType {
    int userId;
    std::string queryResult;

    ResultType(int id, const std::string& result)
        : userId(id), queryResult(result) {}
};

struct QueryTask {
    int userId;
    std::string query;

    QueryTask(int id, const std::string& query)
        : userId(id), query(query) {}
};

// Hàng đợi để lưu các truy vấn
std::queue<QueryTask> queryQueue;
std::mutex queueMutex;
std::condition_variable queueCondition;
bool allQueriesAdded = false; // Cờ báo hiệu tất cả truy vấn đã được thêm vào hàng đợi

// Hàm để các worker threads xử lý truy vấn
void processQueries(std::shared_ptr<ConnectionPool> pool, std::vector<ResultType>& results, std::mutex& resultsMutex) {
    while (true) {
        QueryTask task(0, "");  // khởi tạo để tránh lỗi.

        // Lấy truy vấn từ hàng đợi
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait(lock, [] { return !queryQueue.empty() || allQueriesAdded; }); // chờ đợi nếu có queue

            if (queryQueue.empty() && allQueriesAdded) {
                return;  // Nếu hàng đợi trống và tất cả truy vấn đã được thêm, kết thúc worker thread
            }

            if (!queryQueue.empty()) {
                task = queryQueue.front();
                queryQueue.pop();  // Lấy truy vấn khỏi hàng đợi
            }
        }

        if (!task.query.empty()) {
            // Thực hiện truy vấn khi có kết nối trống
            auto connection = pool->getConnection();
            if (!connection) {
                // Nếu pool đang shutdown và không có kết nối, kết thúc thread
                return;
            }

            connection->execute(task.query);
            std::string result = connection->getResult();

            // Giải phóng kết nối sau khi thực hiện
            pool->releaseConnection(connection);

            // Lưu kết quả
            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                results.emplace_back(task.userId, result);  // Lưu kết quả
            }
        }
    }
}

// Hàm để thêm truy vấn vào hàng đợi
void addQueryToQueue(int userId, const std::string& query) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        queryQueue.push(QueryTask(userId, query));  // Thêm truy vấn vào hàng đợi
    }
    queueCondition.notify_one();  // Thông báo cho worker threads
}

// Hàm để báo hiệu rằng tất cả các truy vấn đã được thêm vào hàng đợi
void stopProcessing() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        allQueriesAdded = true;  // Báo hiệu tất cả truy vấn đã được thêm
    }
    queueCondition.notify_all();  // Đánh thức tất cả worker threads để kết thúc
}

int main() {
    const int maxConnections = 10;
    const int numUsers = 1000;
    const int numWorkers = 5;

    auto pool = std::make_shared<ConnectionPool>("database_connection_string", maxConnections);

    std::vector<ResultType> results;
    std::mutex resultsMutex;

    // Tạo các worker threads
    std::vector<std::thread> workers;
    for (int i = 0; i < numWorkers; ++i) {
        workers.emplace_back(processQueries, pool, std::ref(results), std::ref(resultsMutex));
    }

    // Thêm truy vấn cho các user vào hàng đợi
    for (int userId = 1; userId <= numUsers; ++userId) {
        addQueryToQueue(userId, "select * from test where userId = " + std::to_string(userId));
    }

    // Sau khi thêm tất cả truy vấn vào hàng đợi, gọi stopProcessing
    stopProcessing();

    // Đợi tất cả các worker thread hoàn thành
    for (auto& worker : workers) {
        worker.join();
    }

    // Hiển thị kết quả
    for (const auto& result : results) {
        std::cout << "Kết quả cho User " << result.userId << ": " << result.queryResult << std::endl;
    }

    std::cout << "Size " << results.size() << std::endl;

    std::cout << "Done process !!! ";
    return 0;
}
