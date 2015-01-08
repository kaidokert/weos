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

#ifndef WEOS_COMMON_FUNCTIONAL_HPP
#define WEOS_COMMON_FUNCTIONAL_HPP

#include "../config.hpp"

#include "tuple.hpp" // TODO: wrong path
#include "../type_traits.hpp"
#include "../utility.hpp"

#include <new>


WEOS_BEGIN_NAMESPACE

namespace placeholders
{

template <int TIndex>
struct placeholder
{
};

extern const placeholder<1> _1;
extern const placeholder<2> _2;
extern const placeholder<3> _3;
extern const placeholder<4> _4;
} // namespace placeholders

template <typename T>
struct is_placeholder : WEOS_NAMESPACE::integral_constant<int, 0>
{
};

template <int TIndex>
struct is_placeholder<placeholders::placeholder<TIndex> >
        : WEOS_NAMESPACE::integral_constant<int, TIndex>
{
};

namespace detail
{

// ----=====================================================================----
//     WeakResultType
// ----=====================================================================----

// Checks if T has a member named 'result_type', i.e. if T::result_type exists.
template <typename T>
class has_result_type_member
{
    struct two
    {
        char a;
        char b;
    };

    template <typename U>
    static char test(typename U::result_type* = 0);

    template <typename U>
    static two test(...);

public:
    static const bool value = sizeof(test<T>(0)) == 1;
};

template <typename T, bool THasResultType = has_result_type_member<T>::value>
struct WeakResultTypeHelper
{
    typedef typename T::result_type result_type;
};

template <typename T>
struct WeakResultTypeHelper<T, false>
{
};

// Function object
template <typename T>
struct WeakResultType : public WeakResultTypeHelper<T>
{
};

// Function
template <typename R, typename... TArgs>
struct WeakResultType<R (TArgs...)>
{
    typedef R result_type;
};

// Function reference
template <typename R, typename... TArgs>
struct WeakResultType<R (&)(TArgs...)>
{
    typedef R result_type;
};

// Function pointer
template <typename R, typename... TArgs>
struct WeakResultType<R (*)(TArgs...)>
{
    typedef R result_type;
};

// Member function pointer
template <typename R, typename C, typename... TArgs>
struct WeakResultType<R (C::*) (TArgs...) >
{
    typedef R result_type;
};

// Member function pointer (const)
template <typename R, typename C, typename... TArgs>
struct WeakResultType<R (C::*) (TArgs...) const>
{
    typedef R result_type;
};

// Member function pointer (volatile)
template <typename R, typename C, typename... TArgs>
struct WeakResultType<R (C::*) (TArgs...) volatile>
{
    typedef R result_type;
};

// Member function pointer (const volatile)
template <typename R, typename C, typename... TArgs>
struct WeakResultType<R (C::*) (TArgs...) const volatile>
{
    typedef R result_type;
};

} // namespace detail

// ----=====================================================================----
//     invoke
// ----=====================================================================----

// Invokes a Callable. See: http://en.cppreference.com/w/cpp/concept/Callable

// Pointer to member function + object of/reference to matching class.
template <typename F, typename A0, typename... An>
inline
auto invoke(F&& f, A0&& a0, An&&... an)
    -> decltype((WEOS_NAMESPACE::forward<A0>(a0).*f)(WEOS_NAMESPACE::forward<An>(an)...))
{
    return (WEOS_NAMESPACE::forward<A0>(a0).*f)(WEOS_NAMESPACE::forward<An>(an)...);
}

// Pointer to member function + pointer to matching class.
template <typename F, typename A0, typename... An>
inline
auto invoke(F&& f, A0&& a0, An&&... an)
    -> decltype(((*WEOS_NAMESPACE::forward<A0>(a0)).*f)(WEOS_NAMESPACE::forward<An>(an)...))
{
    return ((*WEOS_NAMESPACE::forward<A0>(a0)).*f)(WEOS_NAMESPACE::forward<An>(an)...);
}

// Pointer to data member + object of/reference to matching class.
template <typename F, typename A0>
inline
auto invoke(F&& f, A0&& a0)
    -> decltype(WEOS_NAMESPACE::forward<A0>(a0).*f)
{
    return WEOS_NAMESPACE::forward<A0>(a0).*f;
}

// Pointer to data member + pointer to matching class.
template <typename F, typename A0>
inline
auto invoke(F&& f, A0&& a0)
    -> decltype((*WEOS_NAMESPACE::forward<A0>(a0)).*f)
{
    return (*WEOS_NAMESPACE::forward<A0>(a0)).*f;
}

// Function object
template <typename F, typename... An>
inline
auto invoke(F&& f, An&&... an)
    -> decltype(WEOS_NAMESPACE::forward<F>(f)(WEOS_NAMESPACE::forward<An>(an)...))
{
    return WEOS_NAMESPACE::forward<F>(f)(WEOS_NAMESPACE::forward<An>(an)...);
}

namespace detail
{

template <typename F, typename... An>
struct invoke_result_type
{
    typedef decltype(invoke(WEOS_NAMESPACE::declval<F>(),
                            WEOS_NAMESPACE::declval<An>()...)) type;
};

// ----=====================================================================----
//     MemFnResult
// ----=====================================================================----

template <typename TMemberPointer>
class MemFnResult : public WeakResultType<TMemberPointer>
{
public:
    MemFnResult(TMemberPointer pm) noexcept
        : m_pm(pm)
    {
    }

    template <typename... TArgs>
    typename invoke_result_type<TMemberPointer, TArgs...>::type
    operator()(TArgs&&... args) const
    {
        return invoke(m_pm, WEOS_NAMESPACE::forward<TArgs>(args)...);
    }

private:
    TMemberPointer m_pm;
};

// ----=====================================================================----
//     BindExpressionResult
// ----=====================================================================----

// -----------------------------------------------------------------------------
// ArgumentSelector
// -----------------------------------------------------------------------------

template <typename TBoundArg, typename TUnboundArgs,
          bool TIsReferenceWrapper = false,
          bool TIsBindExpression = false,
          bool TIsPlaceholderIndex = (is_placeholder<TBoundArg>::value != 0)>
struct ArgumentSelector
{
    static_assert(   TIsReferenceWrapper == false
                  && TIsBindExpression == false
                  && TIsPlaceholderIndex == false,
                  "Logic error");

    typedef TBoundArg& type;

    static type select(TBoundArg& bound, TUnboundArgs& /*unbound*/)
    {
        return bound;
    }
};

template <typename TBoundArg, typename TUnboundArgs>
struct ArgumentSelector<TBoundArg, TUnboundArgs, true, false, false>
{
    //static_assert(false, "Not implemented, yet");
};

template <typename TBoundArg, typename TUnboundArgs>
struct ArgumentSelector<TBoundArg, TUnboundArgs, false, true, false>
{
    //static_assert(false, "Not implemented, yet");
};

struct placeholder_is_out_of_bounds;

template <int TIndex, typename TUnboundArgs,
          bool TValid = (TIndex < tuple_size<TUnboundArgs>::value)>
struct CheckedTypeFromPlaceholder
{
    typedef typename tuple_element<TIndex, TUnboundArgs>::type type;
};

template <int TIndex, typename TUnboundArgs>
struct CheckedTypeFromPlaceholder<TIndex, TUnboundArgs, false>
{
    typedef placeholder_is_out_of_bounds type;
};

template <typename TBoundArg, typename TUnboundArgs>
struct ArgumentSelector<TBoundArg, TUnboundArgs, false, false, true>
{
    static constexpr size_t index = is_placeholder<TBoundArg>::value - 1;
    static_assert(index >= 0, "Placeholder must be positive.");

    typedef typename CheckedTypeFromPlaceholder<index, TUnboundArgs>::type type;

    static type select(TBoundArg& /*bound*/, TUnboundArgs& unbound)
    {
        return WEOS_NAMESPACE::forward<type>(
                /*TODO: WEOS_NAMESPACE::*/get<index>(unbound));
    }
};

template <typename TF, typename TBoundArgs, typename TUnboundArgs>
struct bind_expression_result_type;

template <typename TF, typename... TBoundArgs, typename TUnboundArgs>
struct bind_expression_result_type<TF, tuple<TBoundArgs...>, TUnboundArgs>
{
    typedef decltype(invoke(WEOS_NAMESPACE::declval<TF&>(),
                            WEOS_NAMESPACE::declval<typename ArgumentSelector<TBoundArgs, TUnboundArgs>::type>()...))
          type;
};

template <typename TF, typename TBoundArgs, size_t... TBoundIndices, typename TUnboundArgs>
inline
typename bind_expression_result_type<TF, TBoundArgs, TUnboundArgs>::type
invokeBindExpression(TF& functor,
                     TBoundArgs& boundArgs, TupleIndices<TBoundIndices...>,
                     TUnboundArgs&& unboundArgs)
{
    return invoke(functor,
                  ArgumentSelector<typename tuple_element<TBoundIndices, TBoundArgs>::type,
                                   TUnboundArgs>::select(
                      /*TODO: WEOS_NAMESPACE::*/get<TBoundIndices>(boundArgs), unboundArgs)...);
}

// The result of a bind<>() call.
template <typename TFunctor, typename... TBoundArgs>
class BindExpression : public WeakResultType<typename decay<TFunctor>::type>
{
protected:
    typedef typename decay<TFunctor>::type functor_type;
    typedef tuple<typename decay<TBoundArgs>::type...> bound_args_type;
    typedef typename make_tuple_indices<sizeof...(TBoundArgs)>::type bound_indices_type;

public:
    template <typename F, typename... TArgs,
              typename _ = typename enable_if<is_constructible<functor_type, F>::value
                                              && !is_same<typename decay<F>::type,
                                                          BindExpression>::value>::type>
    explicit BindExpression(F&& f, TArgs&&... boundArgs)
        : m_functor(WEOS_NAMESPACE::forward<F>(f)),
          m_boundArgs(WEOS_NAMESPACE::forward<TArgs>(boundArgs)...)
    {
    }

    BindExpression(const BindExpression& other) = default;
    BindExpression& operator=(const BindExpression& other) = default;

    BindExpression(BindExpression&& other)
        : m_functor(WEOS_NAMESPACE::move(other.m_functor)),
          m_boundArgs(WEOS_NAMESPACE::move(other.m_boundArgs))
    {
    }

    BindExpression& operator=(BindExpression&& other)
    {
        m_functor = WEOS_NAMESPACE::move(other.m_functor);
        m_boundArgs = WEOS_NAMESPACE::move(other.m_boundArgs);
        return *this;
    }

    template <typename... TArgs>
    typename bind_expression_result_type<functor_type, bound_args_type, tuple<TArgs&&...>>::type
    operator()(TArgs&&... args)
    {
        return invokeBindExpression(m_functor,
                                    m_boundArgs, bound_indices_type(),
                                    forward_as_tuple(WEOS_NAMESPACE::forward<TArgs>(args)...));
    }

    template <typename... TArgs>
    typename bind_expression_result_type<const functor_type, const bound_args_type, tuple<TArgs&&...>>::type
    operator()(TArgs&&... args) const
    {
        return invokeBindExpression(m_functor,
                                    m_boundArgs, bound_indices_type(),
                                    forward_as_tuple(WEOS_NAMESPACE::forward<TArgs>(args)...));
    }

private:
    functor_type m_functor;
    bound_args_type m_boundArgs;
};

// The result of a bind<TResult>() call.
template <typename TResult, typename TFunctor, typename... TBoundArgs>
class BindExpressionResult : public BindExpression<TFunctor, TBoundArgs...>
{
    typedef BindExpression<TFunctor, TBoundArgs...> base_type;
    typedef typename base_type::functor_type functor_type;

public:
    typedef TResult result_type;

    template <typename F, typename... TArgs,
              typename _ = typename enable_if<is_constructible<functor_type, F>::value
                                              && !is_same<typename decay<F>::type,
                                                          BindExpressionResult>::value>::type>
    explicit BindExpressionResult(F&& f, TArgs&&... boundArgs)
        : base_type(WEOS_NAMESPACE::forward<F>(f),
                    WEOS_NAMESPACE::forward<TArgs>(boundArgs)...)
    {
    }

    BindExpressionResult(const BindExpressionResult& other) = default;
    BindExpressionResult& operator=(const BindExpressionResult& other) = default;

    BindExpressionResult(BindExpressionResult&& other)
        : base_type(WEOS_NAMESPACE::forward<base_type>(other))
    {
    }

    BindExpressionResult& operator=(BindExpressionResult&& other)
    {
        base_type::operator=(WEOS_NAMESPACE::forward<base_type>(other));
        return *this;
    }

    template <typename... TArgs>
    result_type operator()(TArgs&&... args)
    {
        return base_type::operator()(WEOS_NAMESPACE::forward<TArgs>(args)...);
    }

    template <typename... TArgs>
    result_type operator()(TArgs&&... args) const
    {
        return base_type::operator()(WEOS_NAMESPACE::forward<TArgs>(args)...);
    }
};

} // namespace detail

// ----=====================================================================----
//     mem_fn<>
// ----=====================================================================----

template <typename TResult, typename TClass>
inline
detail::MemFnResult<TResult TClass::*> mem_fn(TResult TClass::* pm) noexcept
{
    return detail::MemFnResult<TResult TClass::*>(pm);
}

// ----=====================================================================----
//     bind
// ----=====================================================================----

template <typename F, typename... TBoundArgs>
inline
detail::BindExpression<F, TBoundArgs...> bind(F&& f, TBoundArgs&&... boundArgs)
{
    typedef detail::BindExpression<F, TBoundArgs...> type;
    return type(WEOS_NAMESPACE::forward<F>(f),
                WEOS_NAMESPACE::forward<TBoundArgs>(boundArgs)...);
}

template <typename R, typename F, typename... TBoundArgs>
inline
detail::BindExpressionResult<R, F, TBoundArgs...> bind(F&& f, TBoundArgs&&... boundArgs)
{
    typedef detail::BindExpressionResult<R, F, TBoundArgs...> type;
    return type(WEOS_NAMESPACE::forward<F>(f),
                WEOS_NAMESPACE::forward<TBoundArgs>(boundArgs)...);
}

// ----=====================================================================----
//     function
// ----=====================================================================----

namespace detail
{

class AnonymousClass;

// A small functor.
// This type is just large enough to hold a member function pointer
// plus a pointer to an instance or a function pointer plus an
// argument of pointer size.
struct SmallFunctor
{
    union Callable
    {
        void* objectPointer;
        const void* constObjectPointer;
        void (*functionPointer)();
        void* AnonymousClass::*memberDataPointer;
        void (AnonymousClass::*memberFunctionPointer)();
    };
    union Argument
    {
        void* objectPointer;
        const void* constObjectPointer;
    };

    Callable callable;
    Argument argument;
};
struct SmallFunctorStorage
{
    void* get() { return &data; }
    const void* get() const { return &data; }

    template <typename T>
    T& get() { return *static_cast<T*>(get()); }

    template <typename T>
    const T& get() const { return *static_cast<const T*>(get()); }

    SmallFunctor data;
};

template <typename TSignature>
class InvokerBase;

template <typename TResult, typename... TArgs>
class InvokerBase<TResult(TArgs...)>
{
public:
    InvokerBase() {}
    virtual ~InvokerBase() {}

    // Clones into newly allocated storage.
    virtual InvokerBase* clone() const = 0;
    // Clones into the given memory.
    virtual void clone(InvokerBase* memory) const = 0;

    virtual void destroy() noexcept = 0;
    virtual void destroyAndDeallocate() noexcept = 0;

    virtual TResult operator()(TArgs&&...) = 0;

private:
    InvokerBase(const InvokerBase&) = delete;
    InvokerBase& operator=(const InvokerBase&) = delete;
};

template <typename TCallable, typename TSignature>
class Invoker;

template <typename TCallable, typename TResult, typename... TArgs>
class Invoker<TCallable, TResult(TArgs...)> : public InvokerBase<TResult(TArgs...)>
{
public:
    typedef InvokerBase<TResult(TArgs...)> base_type;

    explicit Invoker(TCallable&& f)
        : m_callable(WEOS_NAMESPACE::move(f))
    {
    }

    explicit Invoker(const TCallable& f)
        : m_callable(f)
    {
    }

    virtual base_type* clone() const override
    {
        return new Invoker(m_callable);
    }

    virtual void clone(base_type* memory) const override
    {
        ::new (memory) Invoker(m_callable);
    }

    virtual void destroy() noexcept override
    {
        m_callable.~TCallable();
    }

    virtual void destroyAndDeallocate() noexcept override
    {
        delete this;
    }

    virtual TResult operator()(TArgs&&... args) override
    {
        return invoke(m_callable, WEOS_NAMESPACE::forward<TArgs>(args)...);
    }

private:
    TCallable m_callable;
};

} // namespace detail

template <typename TSignature>
class function;

template <typename TResult, typename... TArgs>
class function<TResult(TArgs...)>
{
    using invoker_base_type = detail::InvokerBase<TResult(TArgs...)>;
    using storage_type = typename aligned_storage<sizeof(detail::SmallFunctorStorage)>::type;

    static_assert(alignment_of<invoker_base_type>::value <= alignment_of<storage_type>::value
                  && alignment_of<storage_type>::value % alignment_of<invoker_base_type>::value == 0,
                  "Invalid in-place storage");

    template <typename F>
    using invokeResult = decltype(invoke(declval<F&>(), declval<TArgs>()...));

    template <typename F>
    using isCallable = is_convertible<invokeResult<F>, TResult>;

    template <typename F>
    class canStoreInplace
    {
        static const std::size_t smallSize = sizeof(storage_type);
        static const std::size_t smallAlign = alignment_of<storage_type>::value;
    public:
        static constexpr bool value = sizeof(F) <= smallSize
                                      && alignment_of<F>::value <= smallAlign
                                      && (smallAlign % alignment_of<F>::value == 0)
                                      ;//TODO: && is_nothrow_copy_constructible<F>::value;
    };

    template <typename F, bool TFunctionPointer =    is_function<F>::value
                                                  || is_member_pointer<F>::value>
    struct notNull
    {
        static bool check(F fp) { return fp; }
    };

    template <typename F>
    struct notNull<F, false>
    {
        static bool check(const F&) { return true; }
    };

public:
    typedef TResult result_type;

    function() noexcept
        : m_invoker(0)
    {
    }

    function(nullptr_t) noexcept
        : m_invoker(0)
    {
    }

    function(const function& other)
        : m_invoker(0)
    {
        if (other.m_invoker)
        {
            if (other.m_invoker == (const invoker_base_type*)&other.m_storage)
            {
                m_invoker = (invoker_base_type*)&m_storage;
                other.m_invoker->clone(m_invoker);
            }
            else
            {
                m_invoker = other.m_invoker->clone();
            }
        }
    }

    function(function&& other) noexcept
        : m_invoker(0)
    {
        if (other.m_invoker)
        {
            if (other.m_invoker == (const invoker_base_type*)&other.m_storage)
            {
                m_invoker = (invoker_base_type*)&m_storage;
                other.m_invoker->clone(m_invoker);
            }
            else
            {
                m_invoker = other.m_invoker;
                other.m_invoker = 0;
            }
        }
    }

    template <typename TCallable,
              typename _ = typename enable_if<!is_same<typename decay<TCallable>::type,
                                                       function>::value
                                              && isCallable<TCallable>::value>::type>
    function(TCallable f)
        : m_invoker(0)
    {
        if (notNull<TCallable>::check(f))
        {
            typedef detail::Invoker<TCallable, TResult(TArgs...)> invoker_type;
            if (canStoreInplace<TCallable>::value)
            {
                // Copy-construct using placement new.
                m_invoker = (invoker_base_type*)&m_storage;
                ::new (m_invoker) invoker_type(WEOS_NAMESPACE::move(f));
            }
            else
            {
                m_invoker = new invoker_type(WEOS_NAMESPACE::move(f));
            }
        }
    }

    ~function()
    {
        release();
    }

    function& operator=(const function& other)
    {
        function(other).swap(*this);
        return *this;
    }

    function& operator=(function&& other);

    function& operator=(nullptr_t)
    {
        release();
        return *this;
    }

    template <typename TCallable,
              typename _ = typename enable_if<!is_same<typename decay<TCallable>::type,
                                                       function>::value
                                              && isCallable<typename decay<TCallable>::type>::value>::type>
    function& operator=(TCallable&& f)
    {
        function(WEOS_NAMESPACE::forward<TCallable>(f)).swap(*this);
        return *this;
    }

    result_type operator()(TArgs... args) const
    {
        WEOS_ASSERT(m_invoker);
        return (*m_invoker)(WEOS_NAMESPACE::forward<TArgs>(args)...);
    }

    void swap(function& other) noexcept
    {
        if (this == &other)
            return;

        if (m_invoker == (invoker_base_type*)&m_storage
            && other.m_invoker == (invoker_base_type*)&other.m_storage)
        {
            // Both are small. We can only swap via some temporary space.
            storage_type tempStorage;
            invoker_base_type* tempInvoker = (invoker_base_type*)&tempStorage;

            m_invoker->clone(tempInvoker);
            m_invoker->destroy();
            other.m_invoker->clone((invoker_base_type*)&m_storage);
            other.m_invoker->destroy();
            tempInvoker->clone((invoker_base_type*)&other.m_storage);
            tempInvoker->destroy();

            m_invoker = (invoker_base_type*)&m_storage;
            other.m_invoker = (invoker_base_type*)&other.m_storage;
        }
        else if (other.m_invoker == (invoker_base_type*)&other.m_storage)
        {
            // The other function is small. Our internal storage is available.
            other.m_invoker->clone((invoker_base_type*)&m_storage);
            other.m_invoker->destroy();
            other.m_invoker = m_invoker;
            m_invoker = (invoker_base_type*)&m_storage;
        }
        else if (m_invoker == (invoker_base_type*)&m_storage)
        {
            // We are small. The other internal storage is free.
            m_invoker->clone((invoker_base_type*)&other.m_storage);
            m_invoker->destroy();
            m_invoker = other.m_invoker;
            other.m_invoker = (invoker_base_type*)&other.m_storage;
        }
        else
        {
            // Both are large.
            std::swap(m_invoker, other.m_invoker);
        }
    }

    explicit operator bool() const noexcept
    {
        return m_invoker != 0;
    }

private:
    storage_type m_storage;
    invoker_base_type* m_invoker;

    void release()
    {
        if (m_invoker == (invoker_base_type*)&m_storage)
            m_invoker->destroy();
        else if (m_invoker)
            m_invoker->destroyAndDeallocate();
    }
};

WEOS_END_NAMESPACE

#endif // WEOS_COMMON_FUNCTIONAL_HPP
