#include "sqlite/sqlite.h"
#include "log/logmanager.h"
#include <sqlite3.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <iostream>
struct SQLite::Impl {
    std::string db_path;
    std::queue<sqlite3*> available;
    std::mutex mutex;
    std::condition_variable cv;
    bool shutdown = false;
    int pool_size = 5;
    sqlite3* transaction_db = nullptr;  // 当前事务使用的数据库连接
    
    bool Open(sqlite3*& db) {
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc) {
            LOG_MAIN_ERROR_AT("Cannot open database: {}", sqlite3_errmsg(db));
            sqlite3_close(db);
            return false;
        }
        return true;
    }
    
    void Close(sqlite3* db) {
        if (db) {
            sqlite3_close(db);
        }
    }
};

// 构造函数实现
SQLite::SQLite(const std::string& db_path, int pool_size) {
    Init(db_path, pool_size);
}

// 析构函数实现
SQLite::~SQLite() {
    if (impl_) {
        Shutdown();
    }
}

// 移动构造函数
SQLite::SQLite(SQLite&& other) noexcept 
    : impl_(std::move(other.impl_)) {
}

// 移动赋值运算符
SQLite& SQLite::operator=(SQLite&& other) noexcept {
    if (this != &other) {
        if (impl_) {
            Shutdown();
        }
        impl_ = std::move(other.impl_);
    }
    return *this;
}

void SQLite::Init(const std::string& db_path, int pool_size) {
    impl_ = std::make_unique<Impl>();
    impl_->db_path = db_path;
    impl_->pool_size = pool_size;
    
    for (int i = 0; i < pool_size; ++i) {
        sqlite3* db = nullptr;
        if (impl_->Open(db)) {
            impl_->available.push(db);
        }
    }
    
    LOG_MAIN_INFO_AT("SQLite initialized with {} connections", pool_size);
}

void SQLite::Shutdown() {
    std::lock_guard lock(impl_->mutex);
    impl_->shutdown = true;
    
    while (!impl_->available.empty()) {
        auto db = impl_->available.front();
        impl_->available.pop();
        impl_->Close(db);
    }
    
    impl_->cv.notify_all();
    LOG_MAIN_INFO_AT("SQLite shutdown");
}

SQLite::Error SQLite::Execute(const std::string& sql) {
    return ExecuteWithParams(sql, {});
}

SQLite::Error SQLite::ExecuteWithParams(const std::string& sql, const std::vector<std::string>& params) {
    std::unique_lock lock(impl_->mutex);
    
    impl_->cv.wait(lock, [this] { return !impl_->available.empty() || impl_->shutdown; });
    
    if (impl_->shutdown || impl_->available.empty()) {
        return { ErrorCode::SHUTDOWN, "Database is shutdown or no available connection" };
    }
    
    auto db = impl_->available.front();
    impl_->available.pop();
    lock.unlock();
    
    sqlite3_stmt* stmt = nullptr;
    Error error = { ErrorCode::OK, "" };
    // 准备 SQL 语句
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        // 绑定参数（防止 SQL 注入）
        for (size_t i = 0; i < params.size(); ++i) {
            sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
        }
        // 执行 SQL 语句
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            error = { ErrorCode::EXECUTE_FAILED, std::string("Execute error: ") + sqlite3_errmsg(db) };
            LOG_MAIN_ERROR_AT("SQLite execute error: {}", sqlite3_errmsg(db));
        } else {
            int changes = sqlite3_changes(db);
            std::cout << "Execute affected rows: " << changes << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        error = { ErrorCode::PREPARE_STATEMENT_FAILED, std::string("Prepare error: ") + sqlite3_errmsg(db) };
        LOG_MAIN_ERROR_AT("SQLite prepare error: {}", sqlite3_errmsg(db));
    }
    
    lock.lock();
    if (!impl_->shutdown) {
        impl_->available.push(db);
    } else {
        impl_->Close(db);
    }
    impl_->cv.notify_all();
    
    return error;
}

SQLite::Error SQLite::Query(const std::string& sql, RowParser parser) {
    return QueryWithParams(sql, {}, parser);
}

SQLite::Error SQLite::QueryWithParams(const std::string& sql, const std::vector<std::string>& params, RowParser parser) {
    std::unique_lock lock(impl_->mutex);
    
    impl_->cv.wait(lock, [this] { return !impl_->available.empty() || impl_->shutdown; });
    
    if (impl_->shutdown || impl_->available.empty()) {
        return { ErrorCode::SHUTDOWN, "Database is shutdown or no available connection" };
    }
    
    auto db = impl_->available.front();
    impl_->available.pop();
    lock.unlock();
    
    sqlite3_stmt* stmt = nullptr;
    Error error = { ErrorCode::OK, "" };
    
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        for (size_t i = 0; i < params.size(); ++i) {
            sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
        }
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (parser) {
                parser(stmt);
            }
        }
        sqlite3_finalize(stmt);
    } else {
        error = { ErrorCode::QUERY_FAILED, std::string("Query prepare error: ") + sqlite3_errmsg(db) };
        LOG_MAIN_ERROR_AT("SQLite query prepare error: {}", sqlite3_errmsg(db));
    }
    
    lock.lock();
    if (!impl_->shutdown) {
        impl_->available.push(db);
    } else {
        impl_->Close(db);
    }
    impl_->cv.notify_all();
    
    return error;
}

SQLite::Error SQLite::CreateTable(const std::string& table_name, const std::map<std::string, std::string>& columns) {
    std::ostringstream ss;
    ss << "CREATE TABLE IF NOT EXISTS " << table_name << " (";
    
    bool first = true;
    for (const auto& [col, type] : columns) {
        if (!first) ss << ", ";
        ss << col << " " << type;
        first = false;
    }
    ss << ")";
    
    return Execute(ss.str());
}

SQLite::Error SQLite::Insert(const std::string& table, const std::map<std::string, std::string>& values) {
    std::ostringstream ss;
    ss << "INSERT INTO " << table << " (";
    
    std::vector<std::string> keys;
    std::vector<std::string> params;
    
    for (const auto& [key, value] : values) {
        keys.push_back(key);
        params.push_back(value);
    }
    
    for (size_t i = 0; i < keys.size(); ++i) {
        ss << keys[i];
        if (i < keys.size() - 1) ss << ", ";
    }
    
    ss << ") VALUES (";
    for (size_t i = 0; i < params.size(); ++i) {
        ss << "?";
        if (i < params.size() - 1) ss << ", ";
    }
    ss << ")";
    
    return ExecuteWithParams(ss.str(), params);
}

SQLite::Error SQLite::Update(const std::string& table, const std::map<std::string, std::string>& values, const std::string& where, const std::vector<std::string>& where_params) {
    std::ostringstream ss;
    ss << "UPDATE " << table << " SET ";
    
    std::vector<std::string> params;
    bool first = true;
    for (const auto& [key, value] : values) {
        if (!first) ss << ", ";
        ss << key << " = ?";
        params.push_back(value);
        first = false;
    }
    
    ss << " WHERE " << where;
    
    for (const auto& p : where_params) {
        params.push_back(p);
    }
    
    return ExecuteWithParams(ss.str(), params);
}

SQLite::Error SQLite::Delete(const std::string& table, const std::string& where, const std::vector<std::string>& params) {
    std::ostringstream ss;
    ss << "DELETE FROM " << table << " WHERE " << where;
    
    return ExecuteWithParams(ss.str(), params);
}

SQLite::Error SQLite::Select(const std::string& table, const std::vector<std::string>& columns, const std::string& where, const std::vector<std::string>& params, RowParser parser) {
    std::ostringstream ss;
    ss << "SELECT ";
    
    if (columns.empty()) {
        ss << "*";
    } else {
        bool first = true;
        for (const auto& col : columns) {
            if (!first) ss << ", ";
            ss << col;
            first = false;
        }
    }
    
    ss << " FROM " << table << " WHERE " << where;
    
    return QueryWithParams(ss.str(), params, parser);
}

SQLite::Error SQLite::SelectAll(const std::string& table, const std::vector<std::string>& columns, RowParser parser) {
    std::ostringstream ss;
    ss << "SELECT ";
    
    if (columns.empty()) {
        ss << "*";
    } else {
        bool first = true;
        for (const auto& col : columns) {
            if (!first) ss << ", ";
            ss << col;
            first = false;
        }
    }
    
    ss << " FROM " << table;
    
    return Query(ss.str(), parser);
}

// SQLBuilder 实现
SQLite::SQLBuilder& SQLite::SQLBuilder::Select(const std::vector<std::string>& columns) {
    stmtType_ = SQLBuilder::StatementType::SELECT;
    columns_ = columns;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::From(const std::string& table) {
    table_ = table;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::Where(const std::string& condition) {
    where_ = condition;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::WhereAnd(const std::string& condition) {
    if (where_.empty()) {
        where_ = condition;
    } else {
        where_ += " AND " + condition;
    }
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::WhereOr(const std::string& condition) {
    if (where_.empty()) {
        where_ = condition;
    } else {
        where_ += " OR " + condition;
    }
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::OrderBy(const std::string& column, bool ascending) {
    orderBy_ = column + (ascending ? " ASC" : " DESC");
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::Limit(int count, int offset) {
    limit_ = count;
    offset_ = offset;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::AddParam(const std::string& value) {
    params_.push_back(value);
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::AddParams(const std::vector<std::string>& values) {
    params_.insert(params_.end(), values.begin(), values.end());
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::InsertInto(const std::string& table) {
    stmtType_ = SQLBuilder::StatementType::INSERT;
    table_ = table;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::Values(const std::map<std::string, std::string>& values) {
    values_ = values;
    // 将值添加到参数列表
    for (const auto& [key, value] : values_) {
        params_.push_back(value);
    }
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::Update(const std::string& table) {
    stmtType_ = SQLBuilder::StatementType::UPDATE;
    table_ = table;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::Set(const std::map<std::string, std::string>& values) {
    values_ = values;
    // 将值添加到参数列表
    for (const auto& [key, value] : values_) {
        params_.push_back(value);
    }
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::DeleteFrom(const std::string& table) {
    stmtType_ = SQLBuilder::StatementType::DELETE;
    table_ = table;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::CreateTable(const std::string& table, bool ifNotExists) {
    stmtType_ = SQLBuilder::StatementType::CREATE_TABLE;
    table_ = table;
    ifNotExists_ = ifNotExists;
    return *this;
}

SQLite::SQLBuilder& SQLite::SQLBuilder::Column(const std::string& name, const std::string& type, const std::string& constraints) {
    tableColumns_.emplace_back(name, std::make_pair(type, constraints));
    return *this;
}

std::string SQLite::SQLBuilder::GetSQL() const {
    std::ostringstream ss;
    
    switch (stmtType_) {
        case SQLBuilder::StatementType::SELECT: {
            ss << "SELECT ";
            if (columns_.empty()) {
                ss << "*";
            } else {
                for (size_t i = 0; i < columns_.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << columns_[i];
                }
            }
            ss << " FROM " << table_;
            if (!where_.empty()) {
                ss << " WHERE " << where_;
            }
            if (!orderBy_.empty()) {
                ss << " ORDER BY " << orderBy_;
            }
            if (limit_ >= 0) {
                ss << " LIMIT " << limit_;
                if (offset_ > 0) {
                    ss << " OFFSET " << offset_;
                }
            }
            break;
        }
        case SQLBuilder::StatementType::INSERT: {
            ss << "INSERT INTO " << table_ << " (";
            bool first = true;
            for (const auto& [key, value] : values_) {
                if (!first) ss << ", ";
                ss << key;
                first = false;
            }
            ss << ") VALUES (";
            first = true;
            for (size_t i = 0; i < values_.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << "?";
            }
            ss << ")";
            break;
        }
        case SQLBuilder::StatementType::UPDATE: {
            ss << "UPDATE " << table_ << " SET ";
            bool first = true;
            for (const auto& [key, value] : values_) {
                if (!first) ss << ", ";
                ss << key << " = ?";
                first = false;
            }
            if (!where_.empty()) {
                ss << " WHERE " << where_;
            }
            break;
        }
        case SQLBuilder::StatementType::DELETE: {
            ss << "DELETE FROM " << table_;
            if (!where_.empty()) {
                ss << " WHERE " << where_;
            }
            break;
        }
        case SQLBuilder::StatementType::CREATE_TABLE: {
            ss << "CREATE TABLE " << (ifNotExists_ ? "IF NOT EXISTS " : "") << table_ << " (";
            bool first = true;
            for (const auto& [name, type_constraints] : tableColumns_) {
                if (!first) ss << ", ";
                ss << name << " " << type_constraints.first;
                if (!type_constraints.second.empty()) {
                    ss << " " << type_constraints.second;
                }
                first = false;
            }
            ss << ")";
            break;
        }
    }
    
    return ss.str();
}

std::vector<std::string> SQLite::SQLBuilder::GetParams() const {
    return params_;
}

SQLite::Error SQLite::SQLBuilder::Execute(SQLite& db) {
    std::string sql = GetSQL();
    std::vector<std::string> params = GetParams();
    
    if (stmtType_ == SQLBuilder::StatementType::SELECT) {
        return { ErrorCode::INVALID_PARAMS, "Use Query() for SELECT statements" };
    }
    
    return db.ExecuteWithParams(sql, params);
}

SQLite::Error SQLite::SQLBuilder::Query(SQLite& db, RowParser parser) {
    std::string sql = GetSQL();
    std::vector<std::string> params = GetParams();
    
    if (stmtType_ != SQLBuilder::StatementType::SELECT) {
        return { ErrorCode::INVALID_PARAMS, "Use Execute() for non-SELECT statements" };
    }
    
    return db.QueryWithParams(sql, params, parser);
}

void SQLite::SQLBuilder::Reset() {
    stmtType_ = SQLBuilder::StatementType::SELECT;
    table_.clear();
    columns_.clear();
    values_.clear();
    where_.clear();
    orderBy_.clear();
    limit_ = -1;
    offset_ = 0;
    ifNotExists_ = false;
    tableColumns_.clear();
    params_.clear();
}

// 辅助方法实现
std::string SQLite::GetColumnText(void* stmt, int index) {
    sqlite3_stmt* statement = static_cast<sqlite3_stmt*>(stmt);
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, index));
    if (text == nullptr) {
        return "";
    }
    return std::string(text);
}

int SQLite::GetColumnInt(void* stmt, int index) {
    sqlite3_stmt* statement = static_cast<sqlite3_stmt*>(stmt);
    return sqlite3_column_int(statement, index);
}

long long SQLite::GetColumnInt64(void* stmt, int index) {
    sqlite3_stmt* statement = static_cast<sqlite3_stmt*>(stmt);
    return sqlite3_column_int64(statement, index);
}

double SQLite::GetColumnDouble(void* stmt, int index) {
    sqlite3_stmt* statement = static_cast<sqlite3_stmt*>(stmt);
    return sqlite3_column_double(statement, index);
}

std::map<std::string, std::string> SQLite::GetRowMap(void* stmt) {
    sqlite3_stmt* statement = static_cast<sqlite3_stmt*>(stmt);
    std::map<std::string, std::string> row;
    
    int columnCount = sqlite3_column_count(statement);
    for (int i = 0; i < columnCount; ++i) {
        const char* columnName = sqlite3_column_name(statement, i);
        const char* value = reinterpret_cast<const char*>(sqlite3_column_text(statement, i));
        
        if (columnName && value) {
            row[columnName] = value;
        } else if (columnName) {
            row[columnName] = "";
        }
    }
    
    return row;
}

// 事务实现
SQLite::Transaction::Transaction(SQLite& db) : db_(db) {
    auto error = db_.BeginTransaction();
    if (error.code != SQLite::ErrorCode::OK) {
        active_ = false;
        LOG_MAIN_ERROR_AT("Failed to begin transaction: {}", error.message);
    }
}

SQLite::Transaction::~Transaction() {
    if (active_) {
        Rollback();
    }
}

void SQLite::Transaction::Commit() {
    if (active_) {
        auto error = db_.CommitTransaction();
        if (error.code != SQLite::ErrorCode::OK) {
            LOG_MAIN_ERROR_AT("Failed to commit transaction: {}", error.message);
        }
        active_ = false;
    }
}

void SQLite::Transaction::Rollback() {
    if (active_) {
        auto error = db_.RollbackTransaction();
        if (error.code != SQLite::ErrorCode::OK) {
            LOG_MAIN_ERROR_AT("Failed to rollback transaction: {}", error.message);
        }
        active_ = false;
    }
}

SQLite::Error SQLite::BeginTransaction() {
    return Execute("BEGIN TRANSACTION");
}

SQLite::Error SQLite::CommitTransaction() {
    return Execute("COMMIT");
}

SQLite::Error SQLite::RollbackTransaction() {
    return Execute("ROLLBACK");
}

SQLite::Error SQLite::BatchInsert(const std::string& table, const std::vector<std::string>& columns, 
                                   const std::vector<std::vector<std::string>>& rows) {
    if (columns.empty() || rows.empty()) {
        return { ErrorCode::INVALID_PARAMS, "Columns or rows cannot be empty" };
    }
    
    // 构建 INSERT 语句
    std::ostringstream ss;
    ss << "INSERT INTO " << table << " (";
    
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << columns[i];
    }
    
    ss << ") VALUES (";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "?";
    }
    ss << ")";
    
    std::string sql = ss.str();
    
    // 使用事务进行批量插入
    Transaction trans(*this);
    
    for (const auto& row : rows) {
        if (row.size() != columns.size()) {
            return { ErrorCode::INVALID_PARAMS, "Row size doesn't match column count" };
        }
        
        auto error = ExecuteWithParams(sql, row);
        if (error.code != ErrorCode::OK) {
            return error;
        }
    }
    
    trans.Commit();
    return { ErrorCode::OK, "" };
}

SQLite::Error SQLite::Exists(const std::string& table, const std::string& where, 
                             const std::vector<std::string>& params, bool& result) {
    result = false;
    
    std::ostringstream ss;
    ss << "SELECT 1 FROM " << table << " WHERE " << where << " LIMIT 1";
    
    bool found = false;
    auto error = QueryWithParams(ss.str(), params, [&found](void* stmt) {
        found = true;
    });
    
    if (error.code == ErrorCode::OK) {
        result = found;
    }
    
    return error;
}

SQLite::Error SQLite::Count(const std::string& table, const std::string& where, 
                            const std::vector<std::string>& params, int& count) {
    count = 0;
    
    std::ostringstream ss;
    ss << "SELECT COUNT(*) as cnt FROM " << table;
    if (!where.empty()) {
        ss << " WHERE " << where;
    }
    
    auto error = QueryWithParams(ss.str(), params, [&count](void* stmt) {
        count = GetColumnInt(stmt, 0);
    });
    
    return error;
}
