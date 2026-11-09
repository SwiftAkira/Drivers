#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace flux {

// A dedicated thread that owns a device's HID handle. Jobs run strictly in
// order. When no job is pending and an idle hook is installed, the hook runs
// in a tight loop (used for streaming input reports such as live key travel
// and battery/event notifications); the hook should block for a few ms at most.
class IoWorker {
public:
    explicit IoWorker(std::string name);
    ~IoWorker();

    void start();
    void stop();

    void post(std::function<void()> job);
    void setIdleHook(std::function<void()> hook, int idleSleepMs = 0);
    void clearIdleHook();
    size_t pending() const;

private:
    void run();

    std::string m_name;
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::function<void()>> m_jobs;
    std::function<void()> m_idle;
    int m_idleSleepMs = 0;
    std::atomic<bool> m_running{false};
};

} // namespace flux
