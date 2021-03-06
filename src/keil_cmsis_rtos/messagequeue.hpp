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

#ifndef WEOS_KEIL_CMSIS_RTOS_MESSAGEQUEUE_HPP
#define WEOS_KEIL_CMSIS_RTOS_MESSAGEQUEUE_HPP

#include "core.hpp"

#include "../chrono.hpp"
#include "../system_error.hpp"

#include <cstdint>
#include <cstring>
#include <utility>


WEOS_BEGIN_NAMESPACE

//! A message queue.
//! The message_queue is an object to pass elements from one thread to another
//! in a thread-safe manner. The object statically holds the necessary memory.
template <typename TypeT, std::size_t QueueSizeT>
class message_queue
{
    // The CMSIS message queue operates on elements of type uint32_t.
    static_assert(sizeof(TypeT) <= 4,
                  "Implementation limits lement size to 32 bit.");
    static_assert(QueueSizeT > 0, "The queue size must be non-zero.");

public:
    //! The type of the elements transfered via this message queue.
    typedef TypeT element_type;

    //! Creates a message queue.
    //! Creates an empty message queue.
    message_queue()
        : m_id(0)
    {
        // Keil's CMSIS RTOS wants a zero'ed control block type for
        // initialization.
        m_queueData[0] = 0;
        osMessageQDef_t queueDef = { QueueSizeT, m_queueData };
        m_id = osMessageCreate(&queueDef, NULL);
        if (m_id == 0)
            WEOS_THROW_SYSTEM_ERROR(cmsis_error::osErrorOS,
                                    "message_queue::message_queue failed");
    }

    //! Returns the capacity.
    //! Returns the maximum number of elements which the queue can hold.
    std::size_t capacity() const
    {
        return QueueSizeT;
    }

    //! Receives an element from the queue.
    //! Returns the first element from the message queue. If the queue is
    //! empty, the calling thread is blocked until an element is added.
    element_type receive()
    {
        osEvent result = osMessageGet(m_id, osWaitForever);
        if (result.status != osEventMessage)
            WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                    "message_queue::receive failed");

        element_type element;
        std::memcpy(&element, &result.value.p, sizeof(element_type));
        return element;
    }

    //! Tries to receive an element from the queue.
    //! Tries to receive an element from the message queue.
    //! The element is returned together with a boolean, which is set if the
    //! queue was non-empty. If the queue was empty, the boolean is reset and
    //! the returned element is default-constructed.
    std::pair<bool, element_type> try_receive()
    {
        osEvent result = osMessageGet(m_id, 0);
        if (result.status == osOK)
        {
            return std::pair<bool, element_type>(false, element_type());
        }
        else if (result.status != osEventMessage)
        {
            WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(result.status),
                                    "message_queue::try_receive failed");
        }

        element_type element;
        std::memcpy(&element, &result.value.p, sizeof(element_type));
        return std::pair<bool, element_type>(true, element);
    }

    //! Tries to receive an element from the queue.
    //! Tries to receive an element from the message queue within the timeout
    //! duration \p d.
    //! The element is returned together with a boolean, which is set if the
    //! queue was non-empty. If the queue was empty, the boolean is reset and
    //! the returned element is default-constructed.
    template <typename RepT, typename PeriodT>
    std::pair<bool, element_type> try_receive_for(
            const chrono::duration<RepT, PeriodT>& d)
    {
        try_receiver receiver(m_id);
        if (chrono::detail::cmsis_wait<
                RepT, PeriodT, try_receiver>::wait(d, receiver))
        {
            element_type element;
            std::memcpy(&element, &receiver.datum(), sizeof(element_type));
            return std::pair<bool, element_type>(true, element);
        }

        return std::pair<bool, element_type>(false, element_type());
    }

    //! Sends an element via the queue.
    //! Sends the \p element by appending it at the end of the message queue.
    //! If the queue is full, the calling thread is blocked until space becomes
    //! available.
    void send(element_type element)
    {
        std::uint32_t datum = 0;
        std::memcpy(&datum, &element, sizeof(element_type));
        osStatus status = osMessagePut(m_id, datum, osWaitForever);
        if (status != osOK)
            WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(status),
                                    "message_queue::send failed");
    }

    //! Tries to send an element via the queue.
    //! Tries to send the \p element via the queue. If no space was available,
    //! \p false is returned. Otherwise the method returns \p true. The
    //! calling thread is never blocked.
    bool try_send(element_type element)
    {
        std::uint32_t datum = 0;
        std::memcpy(&datum, &element, sizeof(element_type));
        osStatus status = osMessagePut(m_id, datum, 0);
        if (status == osOK)
            return true;

        if (   status != osErrorTimeoutResource
            && status != osErrorResource)
        {
            WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(status),
                                    "message_queue::try_send failed");
        }

        return false;
    }

    //! Tries to send an element via the queue.
    //! Tries to send the given \p element via the queue and returns \p true
    //! if successful. If there is no space available within the
    //! duration \p d, the operation is aborted an \p false is returned.
    template <typename RepT, typename PeriodT>
    bool try_send_for(element_type element,
                      const chrono::duration<RepT, PeriodT>& d)
    {
        std::uint32_t datum = 0;
        std::memcpy(&datum, &element, sizeof(element_type));
        try_sender sender(m_id, datum);
        return chrono::detail::cmsis_wait<
                RepT, PeriodT, try_sender>::wait(d, sender);
    }

private:
    //! The storage for the message queue.
    std::uint32_t m_queueData[4 + QueueSizeT];
    //! The id of the message queue.
    osMessageQId m_id;

    // A helper to wait for an element in the message queue.
    struct try_receiver
    {
        try_receiver(osMessageQId id)
            : m_id(id),
              m_datum(0)
        {
        }

        // Returns the datum which has been read from the queue.
        std::uint32_t& datum()
        {
            return m_datum;
        }

        // Waits up to \p millisec milliseconds for getting an element from
        // the message queue. Returns \p true, if an element has been acquired
        // and no further waiting is necessary.
        bool operator() (std::int32_t millisec)
        {
            osEvent result = osMessageGet(m_id, millisec);
            if (result.status == osEventMessage)
            {
                m_datum = result.value.v;
                return true;
            }

            if (   result.status != osOK
                && result.status != osEventTimeout)
            {
                WEOS_THROW_SYSTEM_ERROR(
                            cmsis_error::cmsis_error_t(result.status),
                            "message_queue::try_receiver failed");
            }

            return false;
        }

    private:
        // A pointer to the message queue from which an element shall be
        // fetched.
        osMessageQId m_id;
        // The element which has been taken from the queue.
        std::uint32_t m_datum;
    };

    // A helper to put an element into the message queue.
    struct try_sender
    {
        try_sender(osMessageQId id, std::uint32_t datum)
            : m_id(id),
              m_datum(datum)
        {
        }

        // Waits up to \p millisec milliseconds for putting an element into
        // the message queue. Returns \p true, if the element has been enqueued
        // and no further waiting is necessary.
        bool operator() (std::int32_t millisec) const
        {
            osStatus status = osMessagePut(m_id, m_datum, millisec);
            if (status == osOK)
                return true;

            if (   status != osErrorTimeoutResource
                && status != osErrorResource)
            {
                WEOS_THROW_SYSTEM_ERROR(cmsis_error::cmsis_error_t(status),
                                        "message_queue::try_sender failed");
            }

            return false;
        }

    private:
        // A pointer to the message queue into which the element shall be put.
        osMessageQId m_id;
        // The datum to be put into the queue.
        std::uint32_t m_datum;
    };
};

WEOS_END_NAMESPACE

#endif // WEOS_KEIL_CMSIS_RTOS_MESSAGEQUEUE_HPP
