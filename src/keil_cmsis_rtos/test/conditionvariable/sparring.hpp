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

#ifndef WEOS_KEIL_CMSIS_RTOS_TEST_SPARRING_HPP
#define WEOS_KEIL_CMSIS_RTOS_TEST_SPARRING_HPP

#include "../../condition_variable.hpp"

struct SparringData
{
    enum Action
    {
        None,
        ConditionVariableWait,
        ConditionVariableTryWait,
        Terminate
    };

    SparringData(weos::mutex& _m, weos::condition_variable& _cv)
        : mutex(_m),
          cv(_cv),
          action(None),
          busy(false),
          notified(false),
          sparringStarted(false)
    {
    }

    weos::mutex& mutex;
    weos::condition_variable& cv;
    volatile Action action;
    volatile bool busy;
    volatile bool notified;
    volatile bool sparringStarted;
};

extern "C" void sparring(const void* arg)
{
    SparringData* data = static_cast<SparringData*>(const_cast<void*>(arg));
    data->sparringStarted = true;

    while (1)
    {
        if (data->action == SparringData::None)
        {
            osDelay(1);
            continue;
        }
        else if (data->action == SparringData::Terminate)
            break;

        data->busy = true;
        switch (data->action)
        {
            case SparringData::ConditionVariableWait:
            {
                weos::unique_lock<weos::mutex> l(data->mutex);
                data->cv.wait(l);
                data->notified = true;
            } break;
            case SparringData::ConditionVariableTryWait:
            {
                weos::unique_lock<weos::mutex> l(data->mutex);
                if (data->cv.wait_for(l, weos::chrono::milliseconds(100))
                    == weos::cv_status::no_timeout)
                {
                    data->notified = true;
                }

            } break;
            default:
                break;
        }
        data->busy = false;
        data->action = SparringData::None;
    }
}
osThreadDef_t sparringThread = {sparring, osPriorityHigh, 1, 0};

#endif // WEOS_KEIL_CMSIS_RTOS_TEST_SPARRING_HPP
