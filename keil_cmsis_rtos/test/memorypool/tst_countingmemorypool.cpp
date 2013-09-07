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

#include "../../memorypool.hpp"

#include "gtest/gtest.h"

#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_smallint.hpp>

#include <set>

TEST(counting_memory_pool, Constructor)
{
    weos::counting_memory_pool<int, 10> p;
    ASSERT_FALSE(p.empty());
    ASSERT_EQ(10, p.capacity());
    ASSERT_EQ(10, p.size());
}

TEST(counting_memory_pool, allocate)
{
    const int POOL_SIZE = 10;
    weos::counting_memory_pool<int, POOL_SIZE> p;
    char* chunks[POOL_SIZE];

    for (int i = 0; i < POOL_SIZE; ++i)
    {
        ASSERT_EQ(10 - i, p.size());
        ASSERT_FALSE(p.empty());
        void* c = p.allocate();
        ASSERT_TRUE(c != 0);
        ASSERT_EQ(10 - i - 1, p.size());

        // Check the alignment of the allocated chunk.
        char* addr = static_cast<char*>(c);
        ASSERT_TRUE(reinterpret_cast<uintptr_t>(addr)
                    % boost::alignment_of<int>::value == 0);

        for (int j = 0; j < i; ++j)
        {
            // No chunk can be returned twice from the pool.
            ASSERT_FALSE(chunks[j] == addr);

            // Chunks must not overlap.
            if (chunks[j] < addr)
                ASSERT_TRUE(chunks[j] + sizeof(int) <= addr);
            if (chunks[j] > addr)
                ASSERT_TRUE(addr + sizeof(int) <= chunks[j]);
        }
        chunks[i] = addr;
    }

    ASSERT_TRUE(p.empty());
}

TEST(counting_memory_pool, try_allocate)
{
    const int POOL_SIZE = 10;
    weos::counting_memory_pool<int, POOL_SIZE> p;

    for (int i = 0; i < POOL_SIZE; ++i)
    {
        void* c = p.try_allocate();
        ASSERT_TRUE(c != 0);
    }
    ASSERT_TRUE(p.empty());

    for (int i = 0; i < POOL_SIZE; ++i)
    {
        ASSERT_TRUE(p.try_allocate() == 0);
    }
}

TEST(counting_memory_pool, allocate_and_free)
{
    const int POOL_SIZE = 10;
    weos::counting_memory_pool<int, POOL_SIZE> p;
    void* chunks[POOL_SIZE];

    for (int j = 1; j <= POOL_SIZE; ++j)
    {
        for (int i = 0; i < j; ++i)
        {
            ASSERT_EQ(10 - i, p.size());
            void* c = p.allocate();
            ASSERT_TRUE(c != 0);
            ASSERT_EQ(10 - i - 1, p.size());
            chunks[i] = c;
        }
        for (int i = 0; i < j; ++i)
        {
            ASSERT_EQ(10 - j + i, p.size());
            p.free(chunks[i]);
            ASSERT_EQ(10 - j + i + 1, p.size());
        }
    }
}

TEST(counting_memory_pool, random_allocate_and_free)
{
    const int POOL_SIZE = 10;
    weos::counting_memory_pool<int, POOL_SIZE> p;
    void* chunks[POOL_SIZE];
    std::set<void*> uniqueChunks;
    int numAllocatedChunks = 0;

    for (int i = 0; i < POOL_SIZE; ++i)
    {
        void* c = p.allocate();
        ASSERT_TRUE(c != 0);
        chunks[i] = c;
        uniqueChunks.insert(c);
    }
    ASSERT_TRUE(p.empty());
    ASSERT_EQ(POOL_SIZE, uniqueChunks.size());
    for (int i = 0; i < POOL_SIZE; ++i)
    {
        p.free(chunks[i]);
        chunks[i] = 0;
    }

    boost::random::minstd_rand generator;
    boost::random::uniform_smallint<> dist(0, POOL_SIZE - 1);
    for (int i = 0; i < 10000; ++i)
    {
        int index = dist(generator);
        if (chunks[index] == 0)
        {
            void* c = p.allocate();
            ASSERT_TRUE(c != 0);
            ASSERT_TRUE(uniqueChunks.find(c) != uniqueChunks.end());
            chunks[index] = c;
            ++numAllocatedChunks;
        }
        else
        {
            p.free(chunks[index]);
            chunks[index] = 0;
            --numAllocatedChunks;
        }
        ASSERT_EQ(POOL_SIZE - numAllocatedChunks, p.size());
    }
}
