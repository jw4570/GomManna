#include "gommanna/db.hpp"

namespace gm {
Db::Db(const std::string& url) {
    const char* keys[] = {"dbname", "connect_timeout", "application_name", "options", nullptr};
    const char* values[] = {url.c_str(), "5", "gommanna-server",
        "-c statement_timeout=5000 -c lock_timeout=3000 -c idle_in_transaction_session_timeout=10000 -c timezone=UTC", nullptr};
    connection_ = PQconnectdbParams(keys, values, 1);
    if (!connection_ || PQstatus(connection_) != CONNECTION_OK) {
        if (connection_) PQfinish(connection_);
        connection_ = nullptr;
        throw ApiError(503, "database_unavailable", "Database connection unavailable");
    }
    if (PQsetClientEncoding(connection_, "UTF8") != 0) {
        PQfinish(connection_); connection_ = nullptr;
        throw ApiError(503, "database_unavailable", "Database encoding configuration failed");
    }
}
Db::~Db() { if (connection_) PQfinish(connection_); }
bool Db::healthy() const {
    return connection_ && PQstatus(connection_) == CONNECTION_OK && PQtransactionStatus(connection_) == PQTRANS_IDLE;
}
Json Db::query(const std::string& sql, const Params& params) {
    std::vector<const char*> values;
    for (const auto& p : params) values.push_back(p ? p->c_str() : nullptr);
    std::unique_ptr<PGresult, decltype(&PQclear)> result(
        PQexecParams(connection_, sql.c_str(), static_cast<int>(values.size()), nullptr,
                     values.data(), nullptr, nullptr, 0), &PQclear);
    if (!result) throw ApiError(503, "database_unavailable", "Database connection unavailable");
    const auto status = PQresultStatus(result.get());
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        const char* state = PQresultErrorField(result.get(), PG_DIAG_SQLSTATE);
        throw DbError(state ? state : "unknown");
    }
    Json rows = Json::array();
    for (int r = 0; r < PQntuples(result.get()); ++r) {
        Json row = Json::object();
        for (int c = 0; c < PQnfields(result.get()); ++c) {
            const char* key = PQfname(result.get(), c);
            if (PQgetisnull(result.get(), r, c)) { row[key] = nullptr; continue; }
            std::string value(PQgetvalue(result.get(), r, c), PQgetlength(result.get(), r, c));
            switch (PQftype(result.get(), c)) {
                case 16: row[key] = value == "t"; break;
                case 20: case 21: case 23: row[key] = std::stoll(value); break;
                case 700: case 701: case 1700: row[key] = std::stod(value); break;
                case 114: case 3802: row[key] = Json::parse(value); break;
                case 1184:
                    // PostgreSQL timestamptz를 Swift에서 읽기 쉬운 UTC ISO 8601로 변환합니다.
                    if (auto space = value.find(' '); space != std::string::npos) value[space] = 'T';
                    if (value.ends_with("+00")) value.replace(value.size() - 3, 3, "Z");
                    row[key] = value;
                    break;
                default: row[key] = value;
            }
        }
        rows.push_back(std::move(row));
    }
    return rows;
}
void Pool::release(std::unique_ptr<Db> db) {
    std::lock_guard lock(mutex_);
    if (db->healthy()) idle_.push_back(std::move(db));
    else --total_;
    available_.notify_one();
}
Pool::Lease Pool::acquire() {
    std::unique_lock lock(mutex_);
    if (!available_.wait_for(lock, std::chrono::seconds(5), [&] { return !idle_.empty() || total_ < limit_; }))
        throw ApiError(503, "server_busy", "Database pool busy");
    while (!idle_.empty()) {
        auto db = std::move(idle_.back()); idle_.pop_back();
        if (db->healthy()) return {this, std::move(db)};
        --total_;
    }
    ++total_;
    lock.unlock();
    try { return {this, std::make_unique<Db>(url_)}; }
    catch (...) { lock.lock(); --total_; available_.notify_one(); throw; }
}
}
