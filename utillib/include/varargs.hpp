#pragma once

/*
 * vararg magic
 *
 */

#include "func.hpp"

namespace jab{
namespace va{

namespace impl{
    
template<typename F, typename... Args>
struct accumulator;

//using arg0 as the initial condition, call ...op(op(arg0, arg1), arg2 )...
//Or, as special cases, call op(arg0), or op(), for 1 or 0 arguments
//F is intended to be one of the X_anything functors, which have those special-cases built in.
//They are, in turn, based on functors of the form: auto op(const T &lhs, const t &rhs)
template<typename F, typename Lhs, typename Rhs, typename... Args>
struct accumulator<F, Lhs, Rhs, Args...>{
    
    //Three or more operands
    static constexpr decltype(auto) accumulate(F f, Lhs &&lhs, Rhs &&rhs, Args&&... args){
        using R = decltype( f(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs)) );
        return accumulator<F, R, Args...>::accumulate(f, f(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs)), 
                std::forward<Args>(args)...);  
    }
};

//2 operands
template<typename F, typename Lhs, typename Rhs>
struct accumulator<F, Lhs, Rhs>{
    static constexpr decltype(auto) accumulate(F f, Lhs &&lhs, Rhs &&rhs){
        return f(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs));
    }
};

//1 operand
template<typename F, typename T>
struct accumulator<F, T>{
    static constexpr decltype(auto) accumulate(F f, T &&v){
        return f( std::forward<T>(v) );
    }
};

//0 operands
template<typename F>
struct accumulator<F>{
    static constexpr decltype(auto) accumulate(F f){ return f(); }
};

}
    
//Apply recursively: F(F(F(arg0, arg1), arg2), ...)
template<typename F, typename... Args>
constexpr decltype(auto) accumulate(F f, Args&&... args){
    return impl::accumulator<F, decltype(std::forward<Args>(args))...>::accumulate(f, std::forward<Args>(args)...);
}

//Logical-or together 1-n varargs
template<typename... Args>
constexpr decltype(auto) logical_or(const Args &... args){
    return accumulate( jab::func::logical_or(), args...);
}

template<typename... Args>
constexpr decltype(auto) logical_and(const Args &... args){
    return accumulate( jab::func::logical_and(), args...);
}

template<typename... Args>
constexpr decltype(auto) sum(const Args &... args){
    return accumulate( jab::func::add(), args... );
}

template<typename... Args>
constexpr decltype(auto) max(const Args &... args){
    return accumulate( jab::func::max_functor(), args... );
}

}
}
