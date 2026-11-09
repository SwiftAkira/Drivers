#include "IoWorker.h"

#include <pthread.h>

#include <chrono>

namespace flux {

IoWorker::IoWorker(std::string name)
    : m_name(std::move(name))
{
}

IoWorker::~IoWorker()
{
    stop();
}

void IoWorker::start()
{
    if (m_running.exchange(true))
        return;
    m_thread = std::thread([this] { run(); });
    pthread_setname_np(m_thread.native_handle(), m_name.substr(0, 15).c_str());
}

void IoWorker::stop()
{
    if (!m_running.exchange(false))
        return;
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void IoWorker::post(std::function<void()> job)
{
    {
        std::lock_guard lock(m_mutex);
        m_jobs.push_back(std::move(job));
    }
    m_cv.notify_one();
}

void IoWorker::setIdleHook(std::function<void()> hook, int idleSleepMs)
{
    {
        std::lock_guard lock(m_mutex);
        m_idle = std::move(hook);
        m_idleSleepMs = idleSleepMs;
    }
    m_cv.notify_one();
}

void IoWorker::clearIdleHook()
{
    std::lock_guard lock(m_mutex);
    m_idle = nullptr;
}

size_t IoWorker::pending() const
{
    std::lock_guard lock(m_mutex);
    return m_jobs.size();
}

void IoWorker::run()
{
    while (m_running) {
        std::function<void()> job;
        std::function<void()> idle;
        int sleepMs = 0;
        {
            std::unique_lock lock(m_mutex);
            if (m_jobs.empty() && !m_idle)
                m_cv.wait(lock, [this] { return !m_running || !m_jobs.empty() || m_idle; });
            if (!m_running)
                break;
            if (!m_jobs.empty()) {
                job = std::move(m_jobs.front());
                m_jobs.pop_front();
            } else {
                idle = m_idle;
                sleepMs = m_idleSleepMs;
            }
        }
        if (job) {
            job();
        } else if (idle) {
            idle();
            if (sleepMs > 0) {
                std::unique_lock lock(m_mutex);
                m_cv.wait_for(lock, std::chrono::milliseconds(sleepMs),
                              [this] { return !m_running || !m_jobs.empty(); });
            }
        }
    }
}

} // namespace flux
