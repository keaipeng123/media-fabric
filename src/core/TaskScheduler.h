#ifndef GB28181_CORE_TASKSCHEDULER_H
#define GB28181_CORE_TASKSCHEDULER_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gb28181 {

class TaskScheduler
{
public:
    TaskScheduler();
    ~TaskScheduler();

    bool scheduleEvery(const std::string& taskName, int intervalSeconds, const std::function<void()>& task);
    void stopAll();
    size_t taskCount() const;

private:
    class ScheduledTask;

    std::vector<std::unique_ptr<ScheduledTask> > m_tasks;
};

} // namespace gb28181

#endif
