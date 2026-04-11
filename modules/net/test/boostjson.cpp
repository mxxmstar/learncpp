#include <boost/json.hpp>
#include <iostream>
#include <vector>
#include <map>

namespace boostjson = boost::json;
namespace json = boostjson;
void basicUsage() {
    std::cout << "boostjson basic usage" << std::endl;

    // 创建一个JSON对象
    boostjson::object obj = {
        {"name", "Alice"},
        {"age", 30},
        {"isStudent", true}
    };

    // 创建 JSON 数组
    boostjson::array arr = {"apple", "banana", "cherry"};

    // 创建 JSON 值
    boostjson::value val = 42;
    boostjson::value strVal = "Hello, World!";
    boostjson::value boolVal = true;
    boostjson::value nullVal = nullptr;

    // 序列化为字符串
    std::string serialized = boostjson::serialize(obj);
    std::cout << "Serialized JSON object: " << serialized << std::endl;

    // 解析 JSON 字符串
    std::string json_str = R"({"name": "Alice", "age": 30, "isStudent": true})";
    boostjson::value parsed = boostjson::parse(json_str);
    std::cout << "Parsed JSON value: " << parsed << std::endl;
}

void objectOperations() {
    std::cout << "=== Object 操作 ===" << std::endl;

    json::object obj;

    // 1. 插入/更新键值对
    obj["name"] = "Alice";
    obj["age"] = 25;
    obj["email"] = "alice@example.com";

    // 2. 访问值
    std::string name = obj.at("name").as_string().c_str();
    int age = obj.at("age").as_int64();
    std::cout << "Name: " << name << ", Age: " << age << std::endl;

    // 3. 检查键是否存在
    if (obj.contains("email")) {
        std::cout << "Email: " << obj["email"].as_string() << std::endl;
    }

    // 4. 获取对象大小
    std::cout << "Object size: " << obj.size() << std::endl;

    // 5. 遍历对象
    for (const auto& [key, value] : obj) {
        std::cout << key << ": " << value << std::endl;
    }

    // 6. 删除键值对
    obj.erase("email");
    std::cout << "After erasing email: " << json::serialize(obj) << std::endl;

    // 7. 清空对象
    obj.clear();
    std::cout << "After clearing: " << obj.empty() << std::endl;

    std::cout << std::endl;
}

void arrayOperations() {
    std::cout << "=== Array 操作 ===" << std::endl;

    json::array arr;

    // 1. 添加元素
    arr.emplace_back("first");
    arr.emplace_back(2);
    arr.emplace_back(3.14);
    arr.push_back(true);

    // 2. 访问元素
    std::cout << "First element: " << arr[0] << std::endl;
    std::cout << "Second element: " << arr.at(1) << std::endl;

    // 3. 修改元素
    arr[0] = "modified";
    std::cout << "Modified first: " << arr[0] << std::endl;

    // 4. 获取数组大小
    std::cout << "Array size: " << arr.size() << std::endl;

    // 5. 遍历数组
    for (const auto& element : arr) {
        std::cout << "Element: " << element << std::endl;
    }

    // 6. 插入元素到指定位置
    arr.insert(arr.begin() + 1, "inserted");
    std::cout << "After insert: " << json::serialize(arr) << std::endl;

    // 7. 删除元素
    arr.erase(arr.begin());
    std::cout << "After erase: " << json::serialize(arr) << std::endl;

    std::cout << std::endl;
}

void valueOperations() {
    std::cout << "=== Value 操作 ===" << std::endl;

    // 1. 不同类型的值
    json::value str_val = "string value";
    json::value int_val = 123;
    json::value double_val = 3.14159;
    json::value bool_val = true;
    json::value null_val = nullptr;

    // 2. 检查值的类型
    std::cout << "str_val is string: " << str_val.is_string() << std::endl;
    std::cout << "int_val is int64: " << int_val.is_int64() << std::endl;
    std::cout << "double_val is double: " << double_val.is_double() << std::endl;
    std::cout << "bool_val is bool: " << bool_val.is_bool() << std::endl;
    std::cout << "null_val is null: " << null_val.is_null() << std::endl;

    // 3. 类型转换
    std::string str = str_val.as_string().c_str();
    int64_t num = int_val.as_int64();
    double dbl = double_val.as_double();
    bool boolean = bool_val.as_bool();

    std::cout << "String: " << str << std::endl;
    std::cout << "Integer: " << num << std::endl;
    std::cout << "Double: " << dbl << std::endl;
    std::cout << "Boolean: " << boolean << std::endl;

    // 4. 类型转换为 JSON 结构
    json::object obj_val;
    obj_val["nested"] = "value";
    json::value nested_obj = obj_val;
    std::cout << "Nested object: " << nested_obj << std::endl;

    json::array arr_val;
    arr_val.emplace_back(1);
    arr_val.emplace_back(2);
    json::value nested_arr = arr_val;
    std::cout << "Nested array: " << nested_arr << std::endl;

    std::cout << std::endl;
}

void parsingAndSerialization() {
    std::cout << "=== 解析和序列化 ===" << std::endl;

    // 1. 解析 JSON 字符串
    std::string json_text = R"({
        "name": "Bob",
        "age": 35,
        "skills": ["C++", "Python", "JavaScript"],
        "address": {
            "street": "123 Main St",
            "city": "Anytown"
        },
        "active": true
    })";

    json::value parsed = json::parse(json_text);
    std::cout << "Parsed JSON: " << parsed << std::endl;

    // 2. 访问嵌套结构
    json::object& obj = parsed.as_object();
    std::string name = obj["name"].as_string().c_str();
    json::array& skills = obj["skills"].as_array();
    json::object& addr = obj["address"].as_object();

    std::cout << "Name: " << name << std::endl;
    std::cout << "Skills: ";
    for (const auto& skill : skills) {
        std::cout << skill.as_string() << " ";
    }
    std::cout << std::endl;
    std::cout << "City: " << addr["city"].as_string() << std::endl;

    // // 3. 序列化美化输出
    // std::string pretty = json::serialize(parsed, true); // true 表示美化输出
    // std::cout << "Pretty printed:\n" << pretty << std::endl;

    // 4. 错误处理
    try {
        std::string invalid_json = R"({"invalid": json)";
        json::value bad_parse = json::parse(invalid_json);
    } catch (const std::exception& e) {
        std::cout << "Parse error caught: " << e.what() << std::endl;
    }

    std::cout << std::endl;
}

void advancedFeatures() {
    std::cout << "=== 高级特性 ===" << std::endl;

    // 1. 使用 monadic 操作
    json::value val = json::parse(R"({"user": {"profile": {"name": "Charlie"}}})");
    
    // 安全访问嵌套值
    if (auto* obj_ptr = val.if_object()) {
        if (obj_ptr->contains("user")) {
            if (auto* user_obj = obj_ptr->at("user").if_object()) {
                if (user_obj->contains("profile")) {
                    if (auto* profile_obj = user_obj->at("profile").if_object()) {
                        if (profile_obj->contains("name")) {
                            std::cout << "Deep access - Name: " << profile_obj->at("name").as_string() << std::endl;
                        }
                    }
                }
            }
        }
    }

    // 2. 自定义解析选项
    json::parse_options options;
    options.allow_comments = true;  // 允许注释
    options.allow_trailing_commas = true;  // 允许尾随逗号

    std::string json_with_comments = R"({
        "commented": "value",  // 这是注释
        "trailing": "comma",   // 这也是注释
    })";

    try {
        // 使用 stream_parser 来应用解析选项 - 修复错误的构造方式
        json::stream_parser p;  // 不带参数构造
        p.reset(); // 可选，重置解析器状态
        p.write(json_with_comments);
        p.finish();
        json::value comment_val = p.release();
        std::cout << "JSON with comments parsed: " << comment_val << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Comment parsing error: " << e.what() << std::endl;
    }

    // 3. 构建复杂的 JSON 结构
    json::object complex_obj;
    complex_obj["users"] = json::array{
        json::object{{"id", 1}, {"name", "User1"}},
        json::object{{"id", 2}, {"name", "User2"}}
    };
    
    complex_obj["metadata"] = json::object{
        {"version", "1.0"},
        {"timestamp", 1634567890}
    };

    std::cout << "Complex structure: " << json::serialize(complex_obj) << std::endl;

    std::cout << std::endl;
}

void performanceTips() {
    std::cout << "=== 性能提示 ===" << std::endl;

    // 1. 使用 memory_resource (如果可用)
    #ifdef BOOST_JSON_USE_STANDALONE_ASIO
    // boost::json::memory_resource* mr = /* ... */;
    // json::value val = json::parse(json_str, storage);
    #endif

    // 2. 重复使用解析器以提高性能
    json::stream_parser p;
    std::string partial_json = R"({"key": "value")";
    p.write(partial_json);
    p.finish();
    json::value result = p.release();
    std::cout << "Stream parser result: " << result << std::endl;

    // 3. 批量操作
    json::array batch_array;
    for (int i = 0; i < 5; ++i) {
        json::object item;
        item["id"] = i;
        item["name"] = "item_" + std::to_string(i);
        batch_array.push_back(std::move(item));
    }
    std::cout << "Batch created array: " << json::serialize(batch_array) << std::endl;

    std::cout << std::endl;
}

int main() {
    std::cout << "Boost.JSON 使用示例" << std::endl;
    std::cout << "===================" << std::endl;

    basicUsage();
    objectOperations();
    arrayOperations();
    valueOperations();
    parsingAndSerialization();
    advancedFeatures();
    performanceTips();

    return 0;
}
