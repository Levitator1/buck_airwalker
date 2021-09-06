#pragma once

/*
 * Various loosely typed predicates
 */

#include <algorithm>

namespace jab{
namespace func{

//
// binary functors for arbitrary types
// assuming inputs are const
struct logical_or{
    // t t
    // t f
    // f t
    //      = lhrs || rhs
    // t    = t
    // f    = f
    // ()   = f
    template<typename Lhs, typename Rhs>
    bool operator()(const Lhs &lhs, const Rhs &rhs = false) const{
        return lhs || rhs;
    }

    inline bool operator()(){
        return false;
    }
};

struct logical_and{
    // t t
    // t f
    // f t
    //      = lhs && rhs
    // t    = t
    // f    = f
    // ()   = f
    template<typename Lhs, typename Rhs>
    auto operator()(const Lhs &lhs, const Rhs &rhs = true ) const{
        return lhs && rhs;
    }

    inline bool operator()(){
        return false;
    }
};

struct add{
    template<typename Lhs, typename Rhs>
    auto operator()(const Lhs &lhs, const Rhs &rhs) const{
        return lhs + rhs;
    }

    //v + 0
    template<typename T>
    T operator()(const T &v){
        return v;
    }

    inline int operator()(){
        return 0;
    }
};

struct subtract{
    template<typename Lhs, typename Rhs>
    auto operator()(const Lhs &lhs, const Rhs &rhs) const{
        return lhs - rhs;
    }

    //v - 0
    template<typename T>
    T operator()(const T &v){
        return v;
    }

    int operator()(){
        return 0;
    }
};

//base for the two binary logical operators, both 'and' and 'or'
namespace impl{
template< typename OP >
class logical_binary_anything{
    
    static constexpr OP op={};
    
public:
    template<typename Lhs, typename Rhs>
    constexpr auto operator()(const Lhs &lhs, const Rhs &rhs) const{
        return op(lhs, rhs);
    }
    
    //For 1 argument return v || v, or v && v
    template<typename T>
    constexpr auto operator()(const T &v) const{
        return op(v, v);
    }
    
    //For zero arguments, return false
    constexpr bool operator()() const{ return false; }
};
}    

struct max_functor{

    template<typename Lhs, typename Rhs>
    auto operator()( Lhs &lhs, Rhs &rhs){
        return lhs >= rhs ? lhs : rhs;
    }

    template<typename Lhs, typename Rhs>
    auto operator()( const Lhs &lhs, const Rhs &rhs){
        return lhs >= rhs ? lhs : rhs;
    }

    template<typename T>
    T operator()(T &v){
        return v;
    }
};

struct min_functor{

    template<typename Lhs, typename Rhs>
    auto operator()( Lhs &lhs, Rhs &rhs){
        return lhs <= rhs ? lhs : rhs;
    }

    template<typename Lhs, typename Rhs>
    auto operator()( const Lhs &lhs, const Rhs &rhs){
        return lhs <= rhs ? lhs : rhs;
    }

    template<typename T>
    T operator()(T &v){
        return v;
    }
};

}    
}
