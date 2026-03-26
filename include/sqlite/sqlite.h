#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <sqlite3.h>
#include <stdexcept>
class SQLite {
public:
    // 错误码定义
    enum class ErrorCode {
        OK = 0,
        DATABASE_OPEN_FAILED,
        PREPARE_STATEMENT_FAILED,
        EXECUTE_FAILED,
        QUERY_FAILED,
        INVALID_PARAMS,
        SHUTDOWN
    };

    // 错误信息结构
    struct Error {
        ErrorCode code;
        std::string message;
    };
    
    using ExecuteCallback = std::function<bool(void* stmt)>;
    using RowParser = std::function<void(void* stmt)>;

    static SQLite& GetInstance();

    void Init(const std::string& db_path, int pool_size = 5);
    void Shutdown();

    Error Execute(const std::string& sql);

    /**
     * @brief 执行带参数的 SQL 语句
     * 
     * 从连接池获取数据库连接，执行带参数的 SQL 语句，处理执行结果和错误，
     * 最后将数据库连接返回连接池或关闭（如果数据库已关闭）。
     * 
     * @param sql 要执行的 SQL 语句
     * @param params SQL 语句的参数列表
     * @return Error 执行结果，包含错误码和错误信息
     * 
     * @details 此函数是线程安全的，使用连接池管理数据库连接，支持参数化查询，
     * 防止 SQL 注入攻击。适用于执行 INSERT、UPDATE、DELETE 等 DML 语句。
     */
    Error ExecuteWithParams(const std::string& sql, const std::vector<std::string>& params);
    Error Query(const std::string& sql, RowParser parser);
    Error QueryWithParams(const std::string& sql, const std::vector<std::string>& params, RowParser parser);

    // 表操作方法（返回错误信息）
    Error CreateTable(const std::string& table_name, const std::map<std::string, std::string>& columns);
    Error Insert(const std::string& table, const std::map<std::string, std::string>& values);
    Error Update(const std::string& table, const std::map<std::string, std::string>& values, const std::string& where, const std::vector<std::string>& where_params);
    Error Delete(const std::string& table, const std::string& where, const std::vector<std::string>& params);
    Error Select(const std::string& table, const std::vector<std::string>& columns, const std::string& where, const std::vector<std::string>& params, RowParser parser);
    Error SelectAll(const std::string& table, const std::vector<std::string>& columns, RowParser parser);
    
    // 辅助方法：从事务中获取列值
    static std::string GetColumnText(void* stmt, int index);
    static int GetColumnInt(void* stmt, int index);
    static double GetColumnDouble(void* stmt, int index);
    static std::map<std::string, std::string> GetRowMap(void* stmt);
    
    // 事务支持
    class Transaction {
    public:
        explicit Transaction(SQLite& db);
        ~Transaction();
        
        void Commit();
        void Rollback();
        bool IsActive() const { return active_; }
        
    private:
        SQLite& db_;
        bool active_ = true;
    };
    
    Error BeginTransaction();
    Error CommitTransaction();
    Error RollbackTransaction();
    
    // 批量操作
    Error BatchInsert(const std::string& table, const std::vector<std::string>& columns, 
                      const std::vector<std::vector<std::string>>& rows);
    
    // 常用查询辅助方法
    Error Exists(const std::string& table, const std::string& where, 
                const std::vector<std::string>& params, bool& result);
    Error Count(const std::string& table, const std::string& where, 
               const std::vector<std::string>& params, int& count);
    
    /**
     * @brief SQL 构建器类，用于构建和执行 SQL 语句
     * 
     * 提供流畅的链式接口，支持构建各种类型的 SQL 语句，包括 SELECT、INSERT、UPDATE、DELETE 和 CREATE TABLE。
     * 支持参数绑定，防止 SQL 注入攻击。
     */
    class SQLBuilder {
    public:
        /**
         * @brief 构建 SELECT 语句
         * @param columns 要查询的列，默认为空（查询所有列）
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& Select(const std::vector<std::string>& columns = {});
        /**
         * @brief 指定查询的表
         * @param table 表名
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& From(const std::string& table);
        /**
         * @brief 添加 WHERE 条件
         * @param condition WHERE 条件
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& Where(const std::string& condition);
        /**
         * @brief 添加 AND 条件
         * @param condition AND 条件
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& WhereAnd(const std::string& condition);
        /**
         * @brief 添加 OR 条件
         * @param condition OR 条件
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& WhereOr(const std::string& condition);
        /**
         * @brief 添加 ORDER BY 子句
         * @param column 排序列名
         * @param ascending 是否升序，默认升序
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& OrderBy(const std::string& column, bool ascending = true);
        /**
         * @brief 添加 LIMIT 子句
         * @param count 要返回的行数
         * @param offset 偏移量，默认0
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& Limit(int count, int offset = 0);
        
        /**
         * @brief 添加参数
         * @param value 参数值
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& AddParam(const std::string& value);
        /**
         * @brief 添加多个参数
         * @param values 参数值向量
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& AddParams(const std::vector<std::string>& values);
        
        /**
         * @brief 构建 INSERT 语句
         * @param table 表名
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& InsertInto(const std::string& table);
        /**
         * @brief 添加 VALUES 子句
         * @param values 值映射，键为列名，值为值
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& Values(const std::map<std::string, std::string>& values);
        
        /**
         * @brief 构建 UPDATE 语句
         * @param table 表名
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& Update(const std::string& table);
        /**
         * @brief 添加 SET 子句
         * @param values 值映射，键为列名，值为值
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& Set(const std::map<std::string, std::string>& values);
        
        /**
         * @brief 构建 DELETE 语句
         * @param table 表名
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& DeleteFrom(const std::string& table);
        
        /**
         * @brief 构建 CREATE TABLE 语句
         * @param table 表名
         * @param ifNotExists 是否仅在表不存在时创建，默认创建表
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& CreateTable(const std::string& table, bool ifNotExists = true);
        /**
         * @brief 添加表列
         * @param name 列名
         * @param type 列类型
         * @param constraints 列约束，默认为空
         * @return SQLBuilder 引用，用于链式调用
         */
        SQLBuilder& Column(const std::string& name, const std::string& type, const std::string& constraints = "");
        
        /**
         * @brief 获取构建的 SQL 语句和参数绑定
         * @return SQL 语句和参数绑定向量
         */
        std::string GetSQL() const;

        /**
         * @brief 获取参数绑定
         * @return 参数列表
         */
        std::vector<std::string> GetParams() const;
        
        /**
         * @brief 执行构建的 SQL 语句
         * @param db 数据库连接
         * @return 执行结果
         */
        Error Execute(SQLite& db);
        /**
         * @brief 执行构建的 SQL 语句并解析结果
         * @param db 数据库连接
         * @param parser 行解析函数
         * @return 执行结果
         */
        Error Query(SQLite& db, RowParser parser);
        
        /**
         * @brief 重置构建器
         */
        void Reset();
 
    private:
        /**
         * @brief 语句类型
         */
        enum class StatementType {
            SELECT,
            INSERT,
            UPDATE,
            DELETE,
            CREATE_TABLE
        };
 
        StatementType stmtType_ = StatementType::SELECT;    ///< 语句类型
        std::string table_;                                ///< 表名
        std::vector<std::string> columns_;                 ///< 列名列表
        std::map<std::string, std::string> values_;         ///< 值映射，键为列名，值为值
        std::string where_;                                ///< WHERE 子句
        std::string orderBy_;                              ///< ORDER BY 子句
        int limit_ = -1;                                  ///< LIMIT 查询结果的最大行数
        int offset_ = 0;                                  ///< OFFSET 过滤掉查询结果的前n行
        bool ifNotExists_ = false;                        ///< 是否仅在表不存在时创建，默认创建表
        std::vector<std::pair<std::string, std::pair<std::string, std::string>>> tableColumns_; ///< name, (type, constraints)
        std::vector<std::string> params_;  // WHERE 条件参数
    };

private:
    SQLite() = default;
    ~SQLite() = default;
    SQLite(const SQLite&) = delete;
    SQLite& operator=(const SQLite&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;

    // 内部执行方法
    Error executeInternal(const std::string& sql, const std::vector<std::string>& params);
    Error queryInternal(const std::string& sql, const std::vector<std::string>& params, RowParser parser);
};

// 异常类
class SQLiteException : public std::runtime_error {
public:
    SQLiteException(SQLite::ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}
    
    SQLite::ErrorCode code() const { return code_; }
    
private:
    SQLite::ErrorCode code_;
};
 
// 辅助函数，将错误转换为异常
inline void throwIfError(const SQLite::Error& error) {
    if (error.code != SQLite::ErrorCode::OK) {
        throw SQLiteException(error.code, error.message);
    }
}