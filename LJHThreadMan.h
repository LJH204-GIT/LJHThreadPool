#pragma once
#include <thread>
#include <vector>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>

constexpr inline double  SCALE = 1.25; // 任务与线程数比例为此时添加线程
constexpr inline int     MAX_THREADS_COUNT = 50; // 最大线程数

class ThreadManager final
{
public:
    enum class ThreadState : size_t
    {
        Running = 1,
        NoTask = 4
    };
    class ThreadNode
    {
    public:
        std::thread _t_hread;
        ThreadState _s_tate = ThreadState::NoTask;
        explicit ThreadNode(ThreadState pa_state) noexcept
            : _s_tate{pa_state} {}
        ThreadNode(ThreadNode&& original) noexcept
        {
            this->_t_hread = std::move(original._t_hread);
            this->_s_tate = original._s_tate;
        }
    };

    explicit ThreadManager(size_t th_num) noexcept
    {
        for (size_t j = 0; j < th_num; j++)
        {
            //ThreadNode node {[this] { ThreadLoopWork(); }, ThreadState::NoTask};
            std::shared_ptr<ThreadNode> node = std::make_shared<ThreadNode>(ThreadState::NoTask);
            std::thread temp {[this, node] { ThreadLoopWork(node); }};
            node->_t_hread = std::move(temp);
            threads_vector->emplace_back(std::move(node));
        }
    }

    void StopPool()
    {
        Stop = true;
        condition.notify_all();
    }

    void AddWork(std::initializer_list<std::function<void()>> funcs)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (Stop) {
            throw std::runtime_error("submit on stopped ThreadPool");
        }
        for (auto it = rbegin(funcs); it != rend(funcs);)
        {
            tasks_queue->emplace(*it);
        }
    }

    int CleanUPThreads(double Reserve)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        size_t numOfThreads {threads_vector->size()};
        size_t numOfTasks {tasks_queue->size()};

        if (Reserve > static_cast<double>(numOfTasks)/static_cast<double>(numOfThreads))
        {
            size_t numOfDelete = numOfThreads - numOfTasks;
            size_t count = 0;
            for (auto it = threads_vector->rbegin(); it != threads_vector->rend();) {
                if ((*it)->_s_tate == ThreadState::NoTask) {
                    threads_vector->erase(it.base());
                    ++count;
                    if (count == numOfDelete)
                    {
                        break;
                    }
                } else {
                    ++it;
                }
            }
        }
        return numOfThreads - numOfTasks;
    }

    ThreadManager(const ThreadManager& original) noexcept = delete;
    ThreadManager(const ThreadManager&& original) noexcept = delete;
    ThreadManager& operator=(const ThreadManager& origin) noexcept = delete;
    ThreadManager& operator=(const ThreadManager&& right) noexcept = delete;
private:
    std::shared_ptr<std::vector<std::shared_ptr<ThreadNode>>> threads_vector = std::make_shared<std::vector<std::shared_ptr<ThreadNode>>>();
    //std::vector<ThreadNode> threads_;
    std::mutex mutex_;
    bool Stop = false;
    //std::queue<std::function<void()>> tasks;
    std::shared_ptr<std::queue<std::function<void()>>> tasks_queue = std::make_shared<std::queue<std::function<void()>>>();
    std::condition_variable condition;

    void ThreadLoopWork(const std::shared_ptr<ThreadNode>& node)
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition.wait(lock, [this, node]
                {
                    bool a = Stop || !tasks_queue->empty();
                    if (!a)
                    {
                        node->_s_tate = ThreadState::NoTask;
                    }
                    return a;
                });
                if (Stop && tasks_queue->empty())
                {
                    return;
                }
                task = std::move(tasks_queue->front());
                tasks_queue->pop();
            }
            node->_s_tate = ThreadState::Running;
            task();
            node->_s_tate = ThreadState::NoTask;

            {
                std::unique_lock<std::mutex> lock(mutex_);
                size_t numOfThreads {threads_vector->size()};
                size_t numOfTasks {tasks_queue->size()};

                if (numOfThreads < MAX_THREADS_COUNT && static_cast<double>(numOfTasks) / static_cast<double>(numOfThreads) >= SCALE)
                {
                    size_t add = numOfTasks - numOfThreads;
                    if (add >= MAX_THREADS_COUNT) {
                        add = MAX_THREADS_COUNT;
                    }
                    for (size_t i = 0; i < add; i++)
                    {
                        std::shared_ptr<ThreadNode> _node = std::make_shared<ThreadNode>(ThreadState::NoTask);
                        std::thread _temp {[this, _node] { ThreadLoopWork(_node); }};
                        _node->_t_hread = std::move(_temp);
                        threads_vector->emplace_back(std::move(_node));
                    }
                }
            }
        }
    }
};

