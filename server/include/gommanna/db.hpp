#pragma once
#include "gommanna/core.hpp"
#include <libpq-fe.h>
#include <condition_variable>
#include <memory>
#include <optional>
#include <vector>

namespace gm {
using Params = std::vector<std::optional<std::string>>;
struct DbError : std::runtime_error {
    std::string state;
    explicit DbError(std::string state) : std::runtime_error("Database request failed"), state(std::move(state)) {}
};
class Db {
    PGconn* connection_ = nullptr;
public:
    explicit Db(const std::string& url);
    ~Db();
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;
    Json query(const std::string& sql, const Params& params = {});
    bool healthy() const;
};
class Transaction {
    Db& db_;
    bool done_ = false;
public:
    explicit Transaction(Db& db) : db_(db) { db_.query("BEGIN"); }
    ~Transaction() { if (!done_) { try { db_.query("ROLLBACK"); } catch (...) {} } }
    void commit() { db_.query("COMMIT"); done_ = true; }
};
class Pool {
    std::string url_;
    size_t limit_, total_ = 0;
    std::mutex mutex_;
    std::condition_variable available_;
    std::vector<std::unique_ptr<Db>> idle_;
    void release(std::unique_ptr<Db> db);
public:
    struct Lease {
        Pool* pool;
        std::unique_ptr<Db> db;
        ~Lease() { if (db) pool->release(std::move(db)); }
        Db& operator*() const { return *db; }
    };
    Pool(std::string url, size_t limit) : url_(std::move(url)), limit_(limit) {}
    Lease acquire();
};
}
