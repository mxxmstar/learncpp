#include "sqlite/sqlite.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "common/log/logmanager.h"
class SQLiteTest {
public:
    void RunAllTests() {
        Setup();
        
        TestConnection();
        TestCreateTable();
        TestInsert();
        TestSelect();
        TestUpdate();
        TestDelete();
        TestSQLBuilder();
        TestTransaction();
        TestBatchInsert();
        TestHelperMethods();
        
        Cleanup();
        
        std::cout << "\n�?All tests passed!" << std::endl;
    }

private:
    std::unique_ptr<SQLite> db_;
    
    void Setup() {
        db_ = std::make_unique<SQLite>(":memory:", 1);  // 使用内存数据库进行测�?
        std::cout << "Setting up test environment...\n" << std::endl;
    }
    
    void Cleanup() {
        db_.reset();  // 自动调用析构函数�?Shutdown()
        std::cout << "Cleaning up test environment...\n" << std::endl;
    }
    
    void TestConnection() {
        std::cout << "Testing connection...\n";
        auto error = db_->Query("SELECT 1", [](void* stmt) {});
        assert(error.code == SQLite::ErrorCode::OK);
        std::cout << "OK" << std::endl;
    }
    
    void TestCreateTable() {
        std::cout << "Testing CREATE TABLE...\n ";
        std::map<std::string, std::string> columns = {
            {"id", "INTEGER PRIMARY KEY AUTOINCREMENT"},
            {"name", "TEXT NOT NULL"},
            {"value", "REAL"}
        };
        
        auto error = db_->CreateTable("test_table", columns);
        assert(error.code == SQLite::ErrorCode::OK);
        std::cout << "OK" << std::endl;
    }
    
    void TestInsert() {
        std::cout << "Testing INSERT...\n ";
        
        // 插入测试数据
        auto error = db_->Insert("test_table", {
            {"name", "'test1'"},
            {"value", "100.5"}
        });
        assert(error.code == SQLite::ErrorCode::OK);
        
        error = db_->Insert("test_table", {
            {"name", "'test2'"},
            {"value", "200.75"}
        });
        assert(error.code == SQLite::ErrorCode::OK);
        
        error = db_->Insert("test_table", {
            {"name", "'test3'"},
            {"value", "300.25"}
        });
        assert(error.code == SQLite::ErrorCode::OK);

        // auto error = db_->Insert("test_table", {
        //     {"name", "test1"},
        //     {"value", "100.5"}
        // });
        // assert(error.code == SQLite::ErrorCode::OK);
        
        std::cout << "OK" << std::endl;
    }
    
    void TestSelect() {
        std::cout << "Testing SELECT...\n ";
        
        int count = 0;
        auto error = db_->Query("SELECT * FROM test_table", [&count](void* stmt) {
            count++;
        });
        
        assert(error.code == SQLite::ErrorCode::OK);
        assert(count == 3);  // 应该�?3 条记�?
        
        std::cout << "OK" << std::endl;
    }
    
    void TestUpdate() {
        std::cout << "Testing UPDATE...\n ";
        
        // 先查询原始数�?
        std::cout << "Before update:" << std::endl;
        db_->Query("SELECT name, value FROM test_table", 
            [](void* stmt) {
                std::cout << "  name=" << SQLite::GetColumnText(stmt, 0) 
                          << ", value=" << SQLite::GetColumnText(stmt, 1) << std::endl;
            });
        
        auto error = db_->Update("test_table", 
                               {{"value", "999.99"}}, 
                               "name = ?", 
                               {"'test1'"});
        std::cout << "Update error.code=" << (int)error.code << std::endl;
        assert(error.code == SQLite::ErrorCode::OK);
        
        // 验证更新
        bool updated = false;
        error = db_->QueryWithParams("SELECT value FROM test_table WHERE name = ?", 
                         {"'test1'"}, 
                         [&updated](void* stmt) {
                             std::string value = SQLite::GetColumnText(stmt, 0);
                             std::cout << "After update: value=" << value << std::endl;
                             updated = (value == "999.99");
                         });
        
        std::cout << "Query error.code=" << (int)error.code << ", updated=" << updated << std::endl;
        assert(updated);
        
        std::cout << "OK" << std::endl;
    }
    
    void TestDelete() {
        std::cout << "Testing DELETE...\n ";
        
        auto error = db_->Delete("test_table", "name = ?", {"'test2'"});
        assert(error.code == SQLite::ErrorCode::OK);
        
        // 验证删除
        int count = 0;
        error = db_->Query("SELECT COUNT(*) FROM test_table", [&count](void* stmt) {
            count = SQLite::GetColumnInt(stmt, 0);
        });
        
        assert(error.code == SQLite::ErrorCode::OK);

        // db_->Query("SELECT name, value FROM test_table", 
        //     [](void* stmt) {
        //         std::cout << "  name=" << SQLite::GetColumnText(stmt, 0) 
        //                   << ", value=" << SQLite::GetColumnText(stmt, 1) << std::endl;
        //     });

        assert(count == 2);  // 应该剩下 2 条记�?
        
        std::cout << "OK" << std::endl;
    }
    
    void TestSQLBuilder() {
        std::cout << "Testing SQL Builder...\n ";
        
        SQLite::SQLBuilder builder;
        SQLite::Error error;
        
        // 测试 SELECT
        int selectCount = 0;
        builder.Reset();
        builder.Select({"id", "name"})
              .From("test_table")
              .WhereAnd("value > ?")
              .OrderBy("name", true)
              .Limit(10)
              .AddParam("50");
        error = builder.Query(*db_, [&selectCount](void* stmt) {
            selectCount++;
        });
        
        db_->Query("SELECT name, value FROM test_table", 
            [](void* stmt) {
                std::cout << "  name=" << SQLite::GetColumnText(stmt, 0) 
                          << ", value=" << SQLite::GetColumnText(stmt, 1) << std::endl;
            });

        assert(error.code == SQLite::ErrorCode::OK);
        assert(selectCount == 2);
        
        // 测试 INSERT
        builder.Reset();
        builder.InsertInto("test_table")
              .Values({{"name", "'builder_test'"}, {"value", "500.0"}});
        error = builder.Execute(*db_);
        assert(error.code == SQLite::ErrorCode::OK);
        
        // 测试 UPDATE
        builder.Reset();
        builder.Update("test_table")
              .Set({{"value", "600.0"}})
              .Where("name = ?")
              .AddParam("builder_test");
        error = builder.Execute(*db_);
        assert(error.code == SQLite::ErrorCode::OK);
        
        // 测试 DELETE
        builder.Reset();
        builder.DeleteFrom("test_table")
              .Where("name = ?")
              .AddParam("builder_test");
        error = builder.Execute(*db_);
        assert(error.code == SQLite::ErrorCode::OK);
        
        std::cout << "OK" << std::endl;
    }
    
    void TestTransaction() {
        std::cout << "Testing Transaction...\n ";
        
        {
            SQLite::Transaction trans(*db_);
            
            db_->Insert("test_table", {{"name", "'trans1'"}, {"value", "100.0"}});
            db_->Insert("test_table", {{"name", "'trans2'"}, {"value", "200.0"}});
            
            trans.Commit();
        }
        
        // 验证事务提交
        int count = 0;
        auto error = db_->QueryWithParams("SELECT COUNT(*) FROM test_table WHERE name IN (?, ?)", 
                              {"'trans1'", "'trans2'"}, 
                              [&count](void* stmt) {
                                  count = SQLite::GetColumnInt(stmt, 0);
                              });
        
        assert(error.code == SQLite::ErrorCode::OK);
        assert(count == 2);
        
        std::cout << "OK" << std::endl;
    }
    
    void TestBatchInsert() {
        std::cout << "Testing Batch Insert...\n ";
        
        std::vector<std::string> columns = {"name", "value"};
        std::vector<std::vector<std::string>> rows = {
            {"'batch1'", "10.0"},
            {"'batch2'", "20.0"},
            {"'batch3'", "30.0"}
        };
        
        auto error = db_->BatchInsert("test_table", columns, rows);
        assert(error.code == SQLite::ErrorCode::OK);
        
        // 验证批量插入
        int count = 0;
        error = db_->QueryWithParams("SELECT COUNT(*) FROM test_table WHERE name LIKE ?", 
                         {"'batch%'"}, 
                         [&count](void* stmt) {
                             count = SQLite::GetColumnInt(stmt, 0);
                         });
        
        assert(error.code == SQLite::ErrorCode::OK);
        assert(count == 3);
        
        std::cout << "OK" << std::endl;
    }
    
    void TestHelperMethods() {
        std::cout << "Testing Helper Methods...\n ";
        
        // 测试 Exists
        bool exists = false;
        auto error = db_->Exists("test_table", "name = ?", {"'test1'"}, exists);
        assert(error.code == SQLite::ErrorCode::OK);
        assert(exists);
        
        // 测试 Count
        int count = 0;
        error = db_->Count("test_table", "", {}, count);
        assert(error.code == SQLite::ErrorCode::OK);
        assert(count > 0);
        
        // 测试 GetRowMap
        error = db_->Query("SELECT * FROM test_table LIMIT 1", [](void* stmt) {
            auto row = SQLite::GetRowMap(stmt);
            assert(!row.empty());
            assert(row.find("name") != row.end());
        });
        
        std::cout << "OK" << std::endl;
    }
};

int main() {
    LogManager& log_manager = LogManager::getInstance();
    log_manager.Init();
    std::cout << "========================================" << std::endl;
    std::cout << "SQLite Multi-Instance Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    SQLiteTest test;
    test.RunAllTests();
    
    return 0;
}
