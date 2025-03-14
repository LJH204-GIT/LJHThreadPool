#include "LJHThreadMan.h"
template <typename F>
concept ISLAMBDA = std::is_invocable_r_v<void, F>; // 是否支持调用
template <typename F>
concept ISPOINTER = std::is_pointer_v<F>; // 是否为指针
template <typename T>
concept HasWaitMethod = requires(T t) {
    { t.wait() } /*有无wait方法*/;
};

constexpr size_t __Default_Thread_Num = 30;
constexpr double __ReserveThreads = 0.75; // 线程数大于tasks数的比例

class ThreadPool_Large final {
public:
    explicit ThreadPool_Large() = default;

    void AddTask(std::initializer_list<std::function<void()>> funcs)
    {
        thread_manager_.AddWork(funcs);
    }

    int CleanThread(double Reserve = __ReserveThreads)
    {
        return thread_manager_.CleanUPThreads(Reserve);
    }

    void ClosePool()
    {
        thread_manager_.StopPool();
    }

    ThreadPool_Large(const ThreadPool_Large& original) noexcept = delete;
    ThreadPool_Large(const ThreadPool_Large&& original) noexcept = delete;
    ThreadPool_Large& operator=(const ThreadPool_Large& origin) noexcept = delete;
    ThreadPool_Large& operator=(const ThreadPool_Large&& right) noexcept = delete;

private:
    ThreadManager thread_manager_{__Default_Thread_Num};
};