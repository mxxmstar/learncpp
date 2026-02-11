#include <iostream>
#include <boost/pool/object_pool.hpp>

// 定义一个测试类
class MyObject {
public:
    int id;
    MyObject(int id_) : id(id_) {
        std::cout << "MyObject " << id << " construction" << std::endl;
    }
    ~MyObject() {
        std::cout << "MyObject " << id << " deconstruction" << std::endl;
    }
};

int main() {
    // 1. 创建对象池（管理 MyObject 类型）
    boost::object_pool<MyObject> obj_pool;

    // 2. 从池中分配并构造对象（参数传递给构造函数）
    MyObject* obj1 = obj_pool.construct(1); // 构造 id=1 的对象
    MyObject* obj2 = obj_pool.construct(2); // 构造 id=2 的对象

    // 3. 使用对象
    std::cout << "obj1 id: " << obj1->id << std::endl;
    std::cout << "obj2 id: " << obj2->id << std::endl;

    // 4. 销毁单个对象（归还到池中，不释放内存）
    obj_pool.destroy(obj1); // 调用 obj1 的析构函数，内存归池
    obj_pool.destroy(obj2); // 调用 obj2 的析构函数，内存归池

    // 出作用域自动析构    
    return 0;
}