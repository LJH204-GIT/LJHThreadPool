#include <iostream>
#include "LJHThreadPool._oldh"
#include "LJHThreadMan.h"
// ==================== 使用示例 ====================
int main() // 测试无传参版本调用链
{
    // ThreadPool poll{};
    // int testLian = 10;
    //
    // auto ZuiZhongJieGuo = poll.L_then(
    //     poll.Submit_Lambda([i = &testLian]()mutable {
    //         *i = 100;
    //         std::cout << "链条中流动的数据被第一次修改: 100" << std::endl;
    //         std::this_thread::sleep_for(std::chrono::seconds(2));
    //     }, &testLian),
    // [i = &testLian]()
    // {
    //     *i = 200;
    //     std::cout << "链条中流动的数据被第一次修改: 200" << std::endl;
    //     std::this_thread::sleep_for(std::chrono::seconds(3));
    // }, &testLian);
    // std::cout <<"哈哈哈我主线程没被堵塞" <<std::endl;
    // std::cout << *ZuiZhongJieGuo.get() <<std::endl;
    ThreadManager d { 3};

}
//
// int main2() // 测试无传参版本
// {
//     ThreadPool pool{};
//     int test = 90;
//     auto res = pool.Submit_Lambda(
//     [i = &test]()
//     {
//         std::cout << "开始执行 等待5s" <<std::endl;
//         *i = 800;
//         std::this_thread::sleep_for(std::chrono::seconds(5));
//     }
//     , &test);
//     std::cout << "结果: " << *res.get() << std::endl;
// }
// int main1() {
//     ThreadPool pool(4);
//
//     // 示例1: 基础任务提交
//     auto future1 = pool.submit([] {
//         std::cout << "Task 1 开始执行 先睡5s\n";
//         std::this_thread::sleep_for(std::chrono::seconds(5));
//         std::cout << "Task 1 done\n";
//         return 100;
//     });
//
//     // 示例2: 带依赖的任务
//     auto future2 = pool.submit(
//         [&future1]() {
//             auto temp = future1.get();
//             std::cout << "Task 2 received: " << temp << "\n";
//             return temp * 2;
//         }
//     );
//
//
//     auto jjj = pool.then(future2, [](int val) {
//             std::cout << "Task 3 received: " << val << "\n";
//             return "val + 100";
//     });
//
//     // 示例3: 异步链 .then()
//     auto future3 = pool.then(jjj
//             , [](std::string val)
//     {
//         std::cout << "已执行到最后了..." << std::endl;
//         std::cout << "先睡3s" << std::endl;
//         std::this_thread::sleep_for(std::chrono::seconds(3));
//         return val;
//     }
//
//     );
//
//     // 等待最终结果
//     try {
//         std::cout << "Final result: " << future3.get() << "\n";
//     } catch (const std::exception& e) {
//         std::cerr << "Exception: " << e.what() << "\n";
//     }
//
//     return 0;
// }