/*******************************************************************************
  WEOS - Wrapper for embedded operating systems

  Copyright (c) 2013, Manuel Freiberger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  - Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
  - Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef WEOS_CXX11_THREAD_HPP
#define WEOS_CXX11_THREAD_HPP

#include "../config.hpp"
#include "chrono.hpp"

#include <condition_variable>
#include <cstdint>
#include <limits>
#include <mutex>
#include <map>
#include <thread>

namespace weos
{

namespace detail
{
typedef std::uint32_t signal_set;

struct ThreadData
{
    std::mutex signalMutex;
    signal_set signalFlags;
    std::condition_variable signalCv;
};

class ThreadDataManager
{
public:
    ThreadDataManager() {}

    std::shared_ptr<ThreadData> create(std::thread::id id);
    std::shared_ptr<ThreadData> find(std::thread::id id);
    void remove(std::thread::id id);

    static ThreadDataManager& instance();

private:
    ThreadDataManager(const ThreadDataManager&);
    const ThreadDataManager& operator= (const ThreadDataManager&);

    std::mutex m_idToDataMutex;
    std::map<std::thread::id, std::shared_ptr<ThreadData>> m_idToData;
};

} // namespace detail

class thread : public std::thread
{
public:
    thread()
    {
    }

    template <typename TFunction, typename... TArgs>
    explicit thread(TFunction&& f, TArgs&&... args)
        : std::thread(std::forward<TFunction>(f),
                      std::forward<TArgs>(args)...),
          m_data(detail::ThreadDataManager::instance().create(this->get_id()))
    {
    }

    thread(const thread&) = delete;

    thread(thread&& other)
        : m_data(other.m_data)
    {
        other.m_data.reset();
    }

    ~thread()
    {
        detail::ThreadDataManager::instance().remove(this->get_id());
    }

    thread& operator= (thread&) = delete;

    thread& operator= (thread&& other)
    {
        if (this != &other)
        {
            std::thread::operator= (std::move(other));
            m_data = other.m_data;
            other.m_data.reset();
        }
        return *this;
    }

    // -------------------------------------------------------------------------
    // Signal management
    // -------------------------------------------------------------------------

    //! Represents a set of signal flags.
    typedef detail::signal_set signal_set;

    //! Returns the number of signals in a set.
    inline
    static int signals_count()
    {
        return 32;
    }

    //! Returns a signal set with all flags being set.
    inline
    static signal_set all_signals()
    {
        return std::numeric_limits<signal_set>::max();
    }

    //! Clears a set of signals.
    //! Clears the signals which are specified by the \p flags.
    inline
    void clear_signals(signal_set flags)
    {
        if (!joinable())
        {
            //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
        }

        std::lock_guard<std::mutex> lock(m_data->signalMutex);
        m_data->signalFlags &= ~flags;
        m_data->signalCv.notify_one();
    }

    //! Sets a set of signals.
    //! Sets the signals which are specified by the \p flags.
    inline
    void set_signals(signal_set flags)
    {
        if (!joinable())
        {
            //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
        }

        std::lock_guard<std::mutex> lock(m_data->signalMutex);
        m_data->signalFlags |= flags;
        m_data->signalCv.notify_one();
    }

private:
    //! The additional data associated with this thread.
    std::shared_ptr<detail::ThreadData> m_data;
};

namespace this_thread
{
using std::this_thread::get_id;
using std::this_thread::sleep_for;
using std::this_thread::sleep_until;
using std::this_thread::yield;

//! Waits for any signal.
//! Blocks the current thread until one or more signal flags have been set,
//! returns these flags and resets them.
inline
thread::signal_set wait_for_any_signal()
{
    std::shared_ptr<detail::ThreadData> data
            = detail::ThreadDataManager::instance().find(get_id());
    if (!data)
    {
        //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
    }

    std::unique_lock<std::mutex> lock(data->signalMutex);
    if (data->signalFlags == 0)
        data->signalCv.wait(lock, [data]{ return data->signalFlags != 0; });
    thread::signal_set temp = data->signalFlags;
    data->signalFlags = 0;
    return temp;
}

//! Checks if any signal has arrived.
//! Checks if one or more signal flags have been set for the current thread,
//! returns these flags and resets them. If no signal is set, zero
//! is returned.
inline
thread::signal_set try_wait_for_any_signal()
{
    std::shared_ptr<detail::ThreadData> data
            = detail::ThreadDataManager::instance().find(get_id());
    if (!data)
    {
        //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
    }

    std::unique_lock<std::mutex> lock(data->signalMutex);
    thread::signal_set temp = data->signalFlags;
    data->signalFlags = 0;
    return temp;
}


//! Waits until any signal arrives or a timeout occurs.
//! Waits up to the timeout period \p d for one or more signals to be set for
//! the current thread. The set signals will be returned. If the timeout
//! expires, zero is returned.
template <typename RepT, typename PeriodT>
inline
thread::signal_set try_wait_for_any_signal_for(
        const chrono::duration<RepT, PeriodT>& d)
{
    std::shared_ptr<detail::ThreadData> data
            = detail::ThreadDataManager::instance().find(get_id());
    if (!data)
    {
        //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
    }

    std::unique_lock<std::mutex> lock(data->signalMutex);
    if (data->signalFlags == 0)
    {
        if (!data->signalCv.wait_for(
                lock, d, [data]{ return data->signalFlags != 0; }))
        {
            return 0;
        }
    }
    thread::signal_set temp = data->signalFlags;
    data->signalFlags = 0;
    return temp;
}

//! Waits for a set of signals.
//! Blocks the current thread until all signal flags selected by \p flags have
//! been set, returns these flags and resets them. Signal flags which are
//! not selected by \p flags are not reset.
inline
thread::signal_set wait_for_all_signals(thread::signal_set flags)
{
    std::shared_ptr<detail::ThreadData> data
            = detail::ThreadDataManager::instance().find(get_id());
    if (!data)
    {
        //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
    }

    std::unique_lock<std::mutex> lock(data->signalMutex);
    if ((data->signalFlags & flags) != flags)
    {
        data->signalCv.wait(
                lock,
                [data, flags]{ return (data->signalFlags & flags) == flags; });
    }
    data->signalFlags &= ~flags;
    return flags;
}

//! Checks if a set of signals has been set.
//! Checks if all signal flags selected by \p flags have been set, returns
//! these flags and resets them. Signal flags which are not selected
//! through \p flags are not reset.
//! If not all signal flags specified by \p flags are set, zero is returned
//! and no flag is reset.
inline
thread::signal_set try_wait_for_all_signals(
        thread::signal_set flags)
{
    std::shared_ptr<detail::ThreadData> data
            = detail::ThreadDataManager::instance().find(get_id());
    if (!data)
    {
        //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
    }

    std::unique_lock<std::mutex> lock(data->signalMutex);
    thread::signal_set temp = (data->signalFlags & flags) == flags
                              ? flags : 0;
    data->signalFlags &= ~temp;
    return temp;
}

//! Blocks until a set of signals arrives or a timeout occurs.
//! Waits up to the timeout duration \p d for all signals specified by the
//! \p flags to be set. If these signals are set, they are returned and
//! reset. In the case of a timeout, zero is returned and the signal flags
//! are not modified.
template <typename RepT, typename PeriodT>
inline
thread::signal_set try_wait_for_all_signals_for(
        thread::signal_set flags,
        const chrono::duration<RepT, PeriodT>& d)
{
    std::shared_ptr<detail::ThreadData> data
            = detail::ThreadDataManager::instance().find(get_id());
    if (!data)
    {
        //::weos::throw_exception(system_error(-1, cmsis_category())); //! \todo Use correct value
    }

    std::unique_lock<std::mutex> lock(data->signalMutex);
    if ((data->signalFlags & flags) != flags)
    {
        if (!data->signalCv.wait_for(
                lock, d,
                [data, flags]{ return (data->signalFlags & flags) == flags; }))
        {
            return 0;
        }
    }
    data->signalFlags &= ~flags;
    return flags;
}

} // namespace this_thread

} // namespace weos

#endif // WEOS_CXX11_THREAD_HPP
