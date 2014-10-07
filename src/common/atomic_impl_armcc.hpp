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

#ifndef WEOS_COMMON_ATOMIC_IMPL_ARMCC_HPP
#define WEOS_COMMON_ATOMIC_IMPL_ARMCC_HPP


#ifndef WEOS_CONFIG_HPP
    #error "Do not include this file directly."
#endif // WEOS_CONFIG_HPP


WEOS_BEGIN_NAMESPACE

enum memory_order
{
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
};

// ----=====================================================================----
//     atomic_flag
// ----=====================================================================----

#define ATOMIC_FLAG_INIT { 0 }

class atomic_flag
{
public:
    atomic_flag() WEOS_NOEXCEPT
    {
    }

    // Construction from ATOMIC_FLAG_INIT.
    WEOS_CONSTEXPR atomic_flag(bool value) WEOS_NOEXCEPT
      : m_value(value)
    {
    }

    void clear(memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        fetch_and_set(0);
    }

    void clear(memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        fetch_and_set(0);
    }

    bool test_and_set(memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        return fetch_and_set(1);
    }

    bool test_and_set(memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        return fetch_and_set(1);
    }

private:
    volatile int m_value;

    int fetch_and_set(int newValue) WEOS_NOEXCEPT
    {
        int status, oldValue;
        do
        {
            // Read the old value and create a monitor.
            oldValue = __ldrex(&m_value);
            // Try to store the new value in the lock variable. The return value
            // is zero when the write has succeeded.
            status = __strex(newValue, &m_value);
        } while (status != 0);
        // No new memory accesses can be started until the following barrier
        // succeeds.
        __dmb(0xF);
        return oldValue;
    }

    // ---- Hidden methods
    atomic_flag(const atomic_flag&);
    atomic_flag& operator= (const atomic_flag&);
    atomic_flag& operator=(const atomic_flag&) volatile;
};

void atomic_flag_clear(atomic_flag* flag) WEOS_NOEXCEPT
{
    flag->clear();
}

void atomic_flag_clear(volatile atomic_flag* flag) WEOS_NOEXCEPT
{
    flag->clear();
}

void atomic_flag_clear_explicit(atomic_flag* flag, memory_order mo) WEOS_NOEXCEPT
{
    flag->clear(mo);
}

void atomic_flag_clear_explicit(volatile atomic_flag* flag, memory_order mo) WEOS_NOEXCEPT
{
    flag->clear(mo);
}

bool atomic_flag_test_and_set(atomic_flag* flag) WEOS_NOEXCEPT
{
    return flag->test_and_set();
}

bool atomic_flag_test_and_set(volatile atomic_flag* flag) WEOS_NOEXCEPT
{
    return flag->test_and_set();
}

bool atomic_flag_test_and_set_explicit(atomic_flag* flag, memory_order mo) WEOS_NOEXCEPT
{
    return flag->test_and_set(mo);
}

bool atomic_flag_test_and_set_explicit(volatile atomic_flag* flag,
                                       memory_order mo) WEOS_NOEXCEPT
{
    return flag->test_and_set(mo);
}

// ----=====================================================================----
//     atomic_base
// ----=====================================================================----

namespace detail
{

#define WEOS_ATOMIC_MODIFY(op, arg)                                            \
    T old;                                                                     \
    while (1)                                                                  \
    {                                                                          \
        old = (T)__ldrex(&m_value);                                            \
        if (__strex((int)(old op arg), &m_value) == 0)                         \
            break;                                                             \
    }                                                                          \
    __dmb(0xF);                                                                \
    return old


template <typename T>
class atomic_base
{
    static_assert(sizeof(T) <= sizeof(int));

public:
    atomic_base() WEOS_NOEXCEPT
    {
    }

    WEOS_CONSTEXPR atomic_base(T value) WEOS_NOEXCEPT
        : m_value(value)
    {
    }

    T load(memory_order mo = memory_order_seq_cst) const WEOS_NOEXCEPT
    {
        __dmb(0xF);
        return (T)m_value;
    }

    T load(memory_order mo = memory_order_seq_cst) const volatile WEOS_NOEXCEPT
    {
        __dmb(0xF);
        return (T)m_value;
    }

    void store(T value, memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        while (1)
        {
            __ldrex(&m_value);
            if (__strex((int)value, &m_value) == 0)
                break;
        }
        __dmb(0xF);
    }

    void store(T value, memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        while (1)
        {
            __ldrex(&m_value);
            if (__strex((int)value, &m_value) == 0)
                break;
        }
        __dmb(0xF);
    }

    T exchange(T desired, memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        T old;
        while (1)
        {
            old = (T)__ldrex(&m_value);
            if (__strex((int)desired, &m_value) == 0)
                break;
        }
        __dmb(0xF);
        return old;
    }

    T exchange(T desired, memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        T old;
        while (1)
        {
            old = (T)__ldrex(&m_value);
            if (__strex((int)desired, &m_value) == 0)
                break;
        }
        __dmb(0xF);
        return old;
    }

    T fetch_add(T arg, memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(+, arg);
    }

    T fetch_add(T arg, memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(+, arg);
    }

    T fetch_sub(T arg, memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(-, arg);
    }

    T fetch_sub(T arg, memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(-, arg);
    }

    T fetch_and(T arg, memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(&, arg);
    }

    T fetch_and(T arg, memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(&, arg);
    }

    T fetch_or(T arg, memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(|, arg);
    }

    T fetch_or(T arg, memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(|, arg);
    }

    T fetch_xor(T arg, memory_order mo = memory_order_seq_cst) WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(^, arg);
    }

    T fetch_xor(T arg, memory_order mo = memory_order_seq_cst) volatile WEOS_NOEXCEPT
    {
        WEOS_ATOMIC_MODIFY(^, arg);
    }

    T operator= (T value) WEOS_NOEXCEPT
    {
        store(value);
        return value;
    }

    T operator= (T value) volatile WEOS_NOEXCEPT
    {
        store(value);
        return value;
    }

    operator T() const WEOS_NOEXCEPT
    {
        return load();
    }

    operator T() const volatile WEOS_NOEXCEPT
    {
        return load();
    }

private:
    T m_value;

    // ---- Hidden methods
    atomic_base(const atomic_base&);
    atomic_base& operator=(const atomic_base&);
    atomic_base& operator=(const atomic_base&) volatile;
};

} // namespace detail


typedef detail::atomic_base<int>          atomic_int;
typedef detail::atomic_base<unsigned int> atomic_uint;


template <typename T>
class atomic;

template <>
class atomic<int> : public atomic_int
{
};

template <>
class atomic<unsigned int> : public atomic_uint
{
};

WEOS_END_NAMESPACE

#endif // WEOS_COMMON_ATOMIC_IMPL_ARMCC_HPP