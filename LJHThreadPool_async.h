#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>
#include <concepts>

template <typename F>
concept ISLAMBDA = std::is_invocable_r_v<void, F>; // 是否支持调用
template <typename F>
concept ISPOINTER = std::is_pointer_v<F>; // 是否为指针
template <typename T>
concept HasWaitMethod = requires(T t) {
    { t.wait() } /*有无wait方法*/;
};

template <typename L, typename J>
concept LJH = ISLAMBDA<L> && ISPOINTER<J>;

template <typename L, typename J, typename H>
concept LJH2 = HasWaitMethod<L> && ISLAMBDA<J> && ISPOINTER<H>;

class ThreadPool_async final {
public:
    explicit ThreadPool_async(size_t thread_count = std::thread::hardware_concurrency())
        : stop(false) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers.emplace_back([this] { worker_loop(); });
        }
    }

    ~ThreadPool_async() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (auto& worker : workers) {
            worker.join();
        }
    }

    void StopPool()
    {
        stop = true;
        condition.notify_all();
    }
    // 提交一个没有任何参数,返回值的Lambda 所有的处理均放置Lambda中
    // 调用此函数时在第二个参数提供一个指针 调用: Submit_Lambda().get(); 会堵塞到任务完成并返回指针
    template <typename F, typename Re>
     requires LJH<F,Re>
    auto Submit_Lambda(F&& f, Re re)
    {
        using return_type = Re;
        auto promise = std::make_shared<std::promise<return_type>>();
        std::future<return_type> res = promise->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks.emplace([f, promise, re]() mutable
            {
                try
                {
                    f();
                    promise->set_value(re);
                } catch (...)
                {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        condition.notify_one();
        return res;
    }

    template <typename T, typename F,typename Re>
    requires LJH2<T, F, Re>
    auto L_then(T&& f, F&& f2, Re re)
    {
        auto promise = std::make_shared<std::promise<Re>>();
        std::future<Re> next_future = promise->get_future();

        auto wait_task = [&f,f2,re,promise] () mutable
        {
            try
            {
                f.wait();
                f2();
                promise->set_value(re);
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        };
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks.emplace(wait_task);
        }
        condition.notify_one();

        return next_future;
    }



    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));

        /* // auto task = std::make_shared<std::packaged_task<return_type()>>(
        //     //std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        //     [f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable
        //     {
        //         return std::apply(f, std::move(args));
        //     }
        //     );
        //std::future<return_type> res = task->get_future();
        */
        auto task =
            [f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable
            {
                return std::apply(f, std::move(args));
            };  // 将用户丢进来的func和参数通过一些新特性融合到一起 最后直接调用task就可以拿到func的返回值了

        auto promise = std::make_shared<std::promise<return_type>>();
        std::future<return_type> res = promise->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("submit on stopped ThreadPool");
            }
            tasks.emplace([task, promise]() mutable // 重中之重 要写mutable!!!!去捕获形参里的东西
            {
                try
                {
                    if constexpr (std::is_void_v<return_type>)
                    {
                        task();
                        promise->set_value();
                    } else
                    {
                        promise->set_value(task());
                    }
                } catch (...)
                {
                    promise->set_exception(std::current_exception());
                }
            });
        }
        condition.notify_one();
        return res;
    }

    // 异步链支持：.then() 语法
    template <typename T, typename F>
    auto then(std::future<T>& prev_future, F&& func) -> std::future<decltype(func(std::declval<T>()))> {
        using result_type = decltype(func(std::declval<T>()));

        auto promise = std::make_shared<std::promise<result_type>>();
        std::future<result_type> next_future = promise->get_future();

        // 当前置 future 完成时，调度后续任务
        auto wait_task = [&prev_future,func,promise]() mutable {
            try {
                T value = prev_future.get();
                if constexpr (std::is_void_v<result_type>) {
                    func(value);
                    promise->set_value();
                } else {
                    promise->set_value(func(value));
                }
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        };

        submit(wait_task);

        return next_future;
    }
    ThreadPool_async(const ThreadPool_async& original) noexcept = delete;
    ThreadPool_async(const ThreadPool_async&& original) noexcept = delete;
    ThreadPool_async& operator=(const ThreadPool_async& origin) noexcept = delete;
    ThreadPool_async& operator=(const ThreadPool_async&& right) noexcept = delete;
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
    int allRunThreads = 0;
    int RunThreads = 0;

    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                condition.wait(lock, [this] {
                    return stop || !tasks.empty();
                }); // 如果Lambda返回true就继续执行
                    // 锁逻辑: 如果被堵塞就释放锁 如果能继续运行就不释放锁 此锁就能一直加到此作用域结束
                /*
                 * 条件变量的作用:
                 * 如果用户 未执行stop 或 队列为空, 就睡眠
                 * 为了预防死锁 有新任务来时要唤醒一个 调用stop时要唤醒全部
                 */
                if (stop && tasks.empty()) {
                    return;
                }

                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }
};
