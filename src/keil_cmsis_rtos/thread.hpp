/*******************************************************************************
  WEOS - Wrapper for embedded operating systems

  Copyright (c) 2013-2014, Manuel Freiberger
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

#ifndef WEOS_KEIL_CMSIS_RTOS_THREAD_HPP
#define WEOS_KEIL_CMSIS_RTOS_THREAD_HPP

#include "core.hpp"

#include "chrono.hpp"
#include "mutex.hpp"
#include "semaphore.hpp"
#include "system_error.hpp"

#include <cstdint>

WEOS_BEGIN_NAMESPACE

namespace detail
{

//! Traits for native threads.
struct native_thread_traits
{
    // The native type for a thread ID.
    typedef osThreadId thread_id_type;

    // The stack must be able to hold the registers R0-R15.
    static const std::size_t minimum_custom_stack_size = 64;

    static_assert(osFeature_Signals > 0 && osFeature_Signals <= 16,
                  "The wrapper supports only up 16 signals.");

    // Represents a set of signals.
    typedef std::uint16_t signal_set;

    // The number of signals in a set.
    static const int signals_count = osFeature_Signals;

    // A signal set with all flags being set.
    static const signal_set all_signals
                = (std::uint32_t(1) << osFeature_Signals) - 1;

    // Clears the given signal flags of the thread selected by the threadId.
    static void clear_signals(thread_id_type threadId, signal_set flags)
    {
        WEOS_ASSERT(flags <= all_signals);
        std::int32_t result = osSignalClear(threadId, flags);
        WEOS_ASSERT(result >= 0);
        (void)result;
    }

    // Sets the given signal flags of the thread selected by the threadId.
    static void set_signals(thread_id_type threadId, signal_set flags)
    {
        WEOS_ASSERT(flags <= all_signals);
        std::int32_t result = osSignalSet(threadId, flags);
        WEOS_ASSERT(result >= 0);
        (void)result;
    }
};

} // namespace detail

WEOS_END_NAMESPACE


#include "../common/thread_detail.hpp"


WEOS_BEGIN_NAMESPACE

namespace this_thread
{

//! Returns the id of the current thread.
inline
WEOS_NAMESPACE::thread::id get_id()
{
    return WEOS_NAMESPACE::thread::id(osThreadGetId());
}

//! \brief Puts the current thread to sleep.
//!
//! Blocks the execution of the current thread for the given duration \p d.
template <typename RepT, typename PeriodT>
void sleep_for(const chrono::duration<RepT, PeriodT>& d) WEOS_NOEXCEPT
{
    //! \todo Convert to the true amount of ticks, even if the
    //! system does not run with a 1ms tick.
    RepT ticks = chrono::duration_cast<chrono::milliseconds>(d).count();
    if (ticks <= 0)
        ticks = 0;
    else
        ++ticks;

    while (true)
    {
        RepT delay = ticks;
        if (delay > 0xFFFE)
            delay = 0xFFFE;
        ticks -= delay;

        osStatus status = osDelay(delay);
        WEOS_ASSERT(   (ticks == 0 && status == osOK)
                    || (ticks != 0 && status == osEventTimeout));
        (void)status;

        if (delay == 0)
            return;
    }
}

//! \brief Puts the current thread to sleep.
//!
//! Blocks the execution of the current thread until the given \p timePoint.
template <typename ClockT, typename DurationT>
void sleep_until(const chrono::time_point<ClockT, DurationT>& timePoint) WEOS_NOEXCEPT
{
    typedef typename DurationT::rep rep_t;

    while (true)
    {
        //! \todo Convert to the true amount of ticks, even if the
        //! system does not run with a 1ms tick.
        rep_t ticks = chrono::duration_cast<chrono::milliseconds>(
                          timePoint - ClockT::now());
        if (ticks < 0)
            ticks = 0;
        else if (ticks > 0xFFFE)
            ticks = 0xFFFE;

        osStatus status = osDelay(ticks);
        WEOS_ASSERT(   (ticks == 0 && status == osOK)
                    || (ticks != 0 && status == osEventTimeout));
        (void)status;

        if (ticks == 0)
            return;
    }
}

//! Triggers a rescheduling of the executing threads.
inline
void yield()
{
    osStatus status = osThreadYield();
    WEOS_ASSERT(status == osOK);
    (void)status;
}

// ----=====================================================================----
//     Waiting for signals
// ----=====================================================================----

//! Waits for any signal.
//! Blocks the current thread until one or more signal flags have been set,
//! returns these flags and resets them.
inline
thread::signal_set wait_for_any_signal()
{
    osEvent result = osSignalWait(0, osWaitForever);
    if (result.status != osEventSignal)
        WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                "wait_for_signalflags failed");

    return result.value.signals;
}

//! Checks if any signal has arrived.
//! Checks if one or more signal flags have been set for the current thread,
//! returns these flags and resets them. If no signal is set, zero
//! is returned.
inline
thread::signal_set try_wait_for_any_signal()
{
    osEvent result = osSignalWait(0, 0);
    if (result.status == osEventSignal)
    {
        return result.value.signals;
    }

    if (   result.status != osOK
        && result.status != osEventTimeout)
    {
        WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                "try_wait_for_any_signal_until failed");
    }

    return 0;
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
    return try_wait_for_any_signal_until(chrono::steady_clock::now() + d);
}

template <typename ClockT, typename DurationT>
thread::signal_set try_wait_for_any_signal_until(
            const chrono::time_point<ClockT, DurationT>& timePoint)
{
    typedef typename DurationT::rep rep_t;

    while (1)
    {
        //! \todo Convert to the true amount of ticks, even if the
        //! system does not run with a 1ms tick.
        rep_t ticks = chrono::duration_cast<chrono::milliseconds>(
                          timePoint - ClockT::now());
        if (ticks < 0)
            ticks = 0;
        else if (ticks > 0xFFFE)
            ticks = 0xFFFE;

        osEvent result = osSignalWait(0, ticks);
        if (result.status == osEventSignal)
        {
            return result.value.signals;
        }

        if (   result.status != osOK
            && result.status != osEventTimeout)
        {
            WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                    "try_wait_for_any_signal_until failed");
        }

        if (ticks == 0)
            return 0;
    }
}


//! Waits for a set of signals.
//! Blocks the current thread until all signal flags selected by \p flags have
//! been set, returns those flags and resets them. Signal flags which are
//! not selected by \p flags are not reset.
inline
void wait_for_all_signals(thread::signal_set flags)
{
    WEOS_ASSERT(flags > 0 && flags <= thread::all_signals());
    osEvent result = osSignalWait(flags, osWaitForever);
    if (result.status != osEventSignal)
        WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                "wait_for_signalflags failed");
}

//! Checks if a set of signals has been set.
//! Checks if all signal flags selected by \p flags have been set, returns
//! those flags and resets them. Signal flags which are not selected
//! through \p flags are not reset.
//! If not all signal flags specified by \p flags are set, zero is returned
//! and no flag is reset.
inline
bool try_wait_for_all_signals(thread::signal_set flags)
{
    osEvent result = osSignalWait(flags, 0);
    if (result.status == osEventSignal)
    {
        return true;
    }

    if (   result.status != osOK
        && result.status != osEventTimeout)
    {
        WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                "try_wait_for_all_signals failed");
    }

    return false;
}

//! Blocks until a set of signals arrives or a timeout occurs.
//! Waits up to the timeout duration \p d for all signals specified by the
//! \p flags to be set. If these signals are set, they are returned and
//! reset. In the case of a timeout, zero is returned and the signal flags
//! are not modified.
template <typename RepT, typename PeriodT>
inline
bool try_wait_for_all_signals_for(
            thread::signal_set flags,
            const chrono::duration<RepT, PeriodT>& d)
{
    return try_wait_for_all_signals_until(
                flags,
                chrono::steady_clock::now() + d);
}

template <typename ClockT, typename DurationT>
bool try_wait_for_all_signals_until(
            thread::signal_set flags,
            const chrono::time_point<ClockT, DurationT>& timePoint)
{
    WEOS_ASSERT(flags > 0 && flags <= thread::all_signals());

    typedef typename DurationT::rep rep_t;

    while (true)
    {
        //! \todo Convert to the true amount of ticks, even if the
        //! system does not run with a 1ms tick.
        rep_t ticks = chrono::duration_cast<chrono::milliseconds>(
                          timePoint - ClockT::now());
        if (ticks < 0)
            ticks = 0;
        else if (ticks > 0xFFFE)
            ticks = 0xFFFE;

        osEvent result = osSignalWait(flags, ticks);
        if (result.status == osEventSignal)
        {
            return true;
        }

        if (   result.status != osOK
            && result.status != osEventTimeout)
        {
            WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                    "try_wait_for_all_signals_until failed");
        }

        if (ticks == 0)
            return false;
    }
}

} // namespace this_thread

WEOS_END_NAMESPACE

#endif // WEOS_KEIL_CMSIS_RTOS_THREAD_HPP
