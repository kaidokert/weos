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

#ifndef WEOS_OBJECTPOOL_HPP
#define WEOS_OBJECTPOOL_HPP

#include "memorypool.hpp"


WEOS_BEGIN_NAMESPACE

//! An object pool with static (compile-time) storage.
//!
//! The object_pool is a memory pool for (\p TNumElem) objects of
//! type \p TElement. The memory is allocated statically (internally in the
//! object), i.e. the pool does not allocate memory from the heap.
//! In addition to the memory_pool, the object_pool constructs and destroys
//! the allocated objects. When allocating from this pool, the element's
//! constructor is invoked using a placement new. Upon destruction, the
//! element's destructor is called before the memory is returned back to the
//! pool.
template <typename TElement, std::size_t TNumElem>
class object_pool
{
public:
    //! The type of the elements which can be allocated via this pool.
    typedef TElement element_type;

    ~object_pool()
    {
        //! \todo 1. order the free list (insert an order() function into
        //!          the storage
        //!       2. traverse the ordered free list - if the 'next'-pointer
        //!          does not point to the following chunk, the chunk is
        //!          occupied by a live object whose destructor needs to be
        //!          called
    }

    //! Returns the pool's capacity.
    //! Returns the number of elements for which the pool provides memory.
    std::size_t capacity() const WEOS_NOEXCEPT
    {
        return TNumElem;
    }

    //! Checks if the pool is empty.
    //! Returns \p true, if the pool is empty and no more object can be
    //! allocated.
    bool empty() const WEOS_NOEXCEPT
    {
        return m_memoryPool.empty();
    }

    //! Allocates memory for an object.
    //! Allocates memory for one object and returns a pointer to the allocated
    //! space. The object is not initialized, i.e. the caller has to invoke the
    //! constructor using placement-new. The method returns a pointer to
    //! the allocated memory or a null-pointer if no more memory was available.
    element_type* try_allocate()
    {
        return static_cast<element_type*>(m_memoryPool.try_allocate());
    }

    //! Frees a previously allocated storage.
    //! Frees the memory space \p element which has been previously allocated
    //! via this object pool. This function does not destroy the element and
    //! the caller is responsible for calling the destructor, thus.
    void free(element_type* const element)
    {
        m_memoryPool.free(element);
    }

    //! Allocates and constructs an object.
    //! Allocates memory for an object and calls its constructor. The method
    //! returns a pointer to the newly created object or a null-pointer if no
    //! memory was available.
    element_type* try_construct()
    {
        void* mem = this->try_allocate();
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type;
        return element;
    }

    template <class T1>
    element_type* try_construct(WEOS_FWD_REF(T1) x1)
    {
        void* mem = this->try_allocate();
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1));
        return element;
    }

    template <class T1, class T2>
    element_type* try_construct(WEOS_FWD_REF(T1) x1, WEOS_FWD_REF(T2) x2)
    {
        void* mem = this->try_allocate();
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1),
                                    weos::forward<T2>(x2));
        return element;
    }

    //! Destroys an element.
    //! Destroys the \p element whose memory must have been allocated via
    //! this object pool and whose constructor must have been called.
    void destroy(element_type* const element)
    {
        element->~element_type();
        this->free(element);
    }

private:
    typedef memory_pool<TElement, TNumElem> pool_t;
    //! The pool from which the memory for the elements is allocated.
    pool_t m_memoryPool;
};

//! A shared object pool.
//!
//! The shared_object_pool is a pool for the creation of elements of type
//! \p TElement. The necessary memory for up to (\p TNumElem) elements is
//! held internally, i.e. no memory is allocated dynamically.
//!
//! As the shared_object_pool uses a shared_memory_pool internally, it is
//! thread-safe.
template <typename TElement, std::size_t TNumElem>
class shared_object_pool
{
public:
    //! The type of the elements which can be allocated via this pool.
    typedef TElement element_type;

    ~shared_object_pool()
    {
        //! \todo Sort and delete the objects
    }

    //! Returns the pool's capacity.
    //! Returns the number of elements for which the pool provides memory.
    std::size_t capacity() const WEOS_NOEXCEPT
    {
        return TNumElem;
    }

    //! Checks if the pool is empty.
    //! Returns \p true if the pool is empty.
    bool empty() const
    {
        return m_memoryPool.empty();
    }

    //! Returns the number of available elements.
    std::size_t size() const
    {
        return m_memoryPool.size();
    }

    //! Allocates memory for an element.
    //! Allocates memory for an element from the pool and returns a pointer to
    //! it. The calling thread is blocked until an element is available.
    //!
    //! \note The returned element is not constructed, i.e. only the memory for
    //! it has been allocated.
    //!
    //! \sa free(), try_allocate(), try_allocate_for()
    element_type* allocate()
    {
        return static_cast<element_type*>(m_memoryPool.allocate());
    }

    //! Tries to allocate memory for an element.
    //! Tries to allocate memory for an element from the pool and returns a
    //! pointer to it. If no memory is available, a null-pointer is returned.
    //!
    //! \note The returned element is not constructed, i.e. only the memory for
    //! it has been allocated.
    //!
    //! \sa allocate(), free(), try_allocate_for()
    element_type* try_allocate()
    {
        return static_cast<element_type*>(m_memoryPool.try_allocate());
    }

    //! Tries to allocate memory for an element with timeout.
    //! Tries to allocate memory for an element and returns a pointer to it.
    //! If no memory is available, the method blocks the calling thread until
    //! either an element becomes available or the timeout duration \p d
    //! expires. In the latter case, a null-pointer is returned.
    //!
    //! \note The returned element is not constructed, i.e. only the memory for
    //! it has been allocated.
    //!
    //! \sa allocate(), free(), try_allocate()
    template <typename RepT, typename PeriodT>
    element_type* try_allocate_for(const chrono::duration<RepT, PeriodT>& d)
    {
        return static_cast<element_type*>(m_memoryPool.try_allocate_for(d));
    }

    //! Returns an element back to the pool.
    //! Returns the \p element, which must have been allocated from this
    //! pool, back to the pool. The destructor of the element will not be
    //! called---this is the responsibility of the caller.
    //!
    //! \sa allocate(), try_allocate(), try_allocate_for()
    void free(element_type* const element)
    {
        m_memoryPool.free(element);
    }

    //! Constructs an object.
    //! Allocates memory and constructs an element in it. The method
    //! returns a pointer to the newly constructed object. If the pool is empty,
    //! the calling thread is blocked until an element has been returned.
    element_type* construct()
    {
        void* mem = this->allocate();
        element_type* element = new (mem) element_type;
        return element;
    }

    template <class T1>
    element_type* construct(WEOS_FWD_REF(T1) x1)
    {
        void* mem = this->allocate();
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1));
        return element;
    }

    template <class T1, class T2>
    element_type* construct(WEOS_FWD_REF(T1) x1, WEOS_FWD_REF(T2) x2)
    {
        void* mem = this->allocate();
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1),
                                    weos::forward<T2>(x2));
        return element;
    }

    //! Tries to construct an object.
    //! Tries to allocate memory and constructs an element in it. Then
    //! a pointer to the newly constructed element is returned. If no memory
    //! is available in the pool, a null-pointer is returned.
    element_type* try_construct()
    {
        void* mem = this->try_allocate();
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type;
        return element;
    }

    template <class T1>
    element_type* try_construct(WEOS_FWD_REF(T1) x1)
    {
        void* mem = this->try_allocate();
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1));
        return element;
    }

    template <class T1, class T2>
    element_type* try_construct(WEOS_FWD_REF(T1) x1, WEOS_FWD_REF(T2) x2)
    {
        void* mem = this->try_allocate();
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1),
                                    weos::forward<T2>(x2));
        return element;
    }

    //! Tries to construct an object with timeout.
    //! Tries to allocate memory and constructs an element in it. Then
    //! a pointer to the newly constructed element is returned. If no memory
    //! is available in the pool, the calling thread is blocked until either
    //! a memory block becomes available or the timeout duration \p d
    //! expires. In the latter case, a null-pointer is returned.
    template <typename RepT, typename PeriodT>
    element_type* try_construct_for(const chrono::duration<RepT, PeriodT>& d)
    {
        void* mem = this->try_allocate_for(d);
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type;
        return element;
    }

    template <typename RepT, typename PeriodT, class T1>
    element_type* try_construct_for(const chrono::duration<RepT, PeriodT>& d,
                                    WEOS_FWD_REF(T1) x1)
    {
        void* mem = this->try_allocate_for(d);
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1));
        return element;
    }

    template <typename RepT, typename PeriodT, class T1, class T2>
    element_type* try_construct_for(const chrono::duration<RepT, PeriodT>& d,
                                    WEOS_FWD_REF(T1) x1,
                                    WEOS_FWD_REF(T2) x2)
    {
        void* mem = this->try_allocate_for(d);
        if (!mem)
            return 0;
        element_type* element = new (mem) element_type(
                                    weos::forward<T1>(x1),
                                    weos::forward<T2>(x2));
        return element;
    }

    //! Destroys an element.
    //! Destroys the \p element whose memory must have been allocated via
    //! this object pool. The destructor of \p element is called before
    //! its memory is returned.
    //!
    //! \sa construct()
    void destroy(element_type* const element)
    {
        element->~element_type();
        this->free(element);
    }

private:
    typedef shared_memory_pool<TElement, TNumElem> pool_t;
    //! The pool from which the memory for the elements is allocated.
    pool_t m_memoryPool;
};

WEOS_END_NAMESPACE

#endif // WEOS_OBJECTPOOL_HPP
