#include "TaskScheduler.h"
#include "Logger.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace gb28181 {

class TaskScheduler::ScheduledTask
{
public:
    ScheduledTask(const std::string& taskName, int intervalMilliseconds, const std::function<void()>& task)
        : m_taskName(taskName),
          m_intervalMilliseconds(intervalMilliseconds),
          m_task(task),
          m_stopped(false)
    {
    }

    ~ScheduledTask()
    {
        stop();
    }

    bool start()
    {
        if (m_intervalMilliseconds <= 0 || !m_task)
        {
            return false;
        }

        m_thread = std::thread(&ScheduledTask::run, this);
        return true;
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopped.store(true);
        }
        m_wakeup.notify_all();
        if (m_thread.joinable())
        {
            m_thread.join();
        }
    }

private:
    void run()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_stopped.load())
        {
            if (m_wakeup.wait_for(lock, std::chrono::milliseconds(m_intervalMilliseconds), [this]() { return m_stopped.load(); }))
            {
                break;
            }

            lock.unlock();
            m_task();
            lock.lock();
        }
    }

    std::string m_taskName;
    int m_intervalMilliseconds;
    std::function<void()> m_task;
    std::atomic<bool> m_stopped;
    std::mutex m_mutex;
    std::condition_variable m_wakeup;
    std::thread m_thread;
};

TaskScheduler::TaskScheduler()
{
}

TaskScheduler::~TaskScheduler()
{
    stopAll();
}

bool TaskScheduler::scheduleEvery(const std::string& taskName, int intervalSeconds, const std::function<void()>& task)
{
    return scheduleEveryMs(taskName, intervalSeconds * 1000, task);
}

bool TaskScheduler::scheduleEveryMs(const std::string& taskName, int intervalMilliseconds, const std::function<void()>& task)
{
    std::unique_ptr<ScheduledTask> scheduledTask(new ScheduledTask(taskName, intervalMilliseconds, task));
    if (!scheduledTask->start())
    {
        return false;
    }

    media_fabric::Logger::instance().log(media_fabric::LOG_INFO, "scheduler", "task=" + taskName + " interval_ms=" + std::to_string(intervalMilliseconds));
    m_tasks.push_back(std::move(scheduledTask));
    return true;
}

void TaskScheduler::stopAll()
{
    for (std::vector<std::unique_ptr<ScheduledTask> >::iterator it = m_tasks.begin();
         it != m_tasks.end();
         ++it)
    {
        (*it)->stop();
    }
    m_tasks.clear();
}

size_t TaskScheduler::taskCount() const
{
    return m_tasks.size();
}

} // namespace gb28181
