#pragma once

#include <functional>
#include <mutex>
#include <iostream>
#include "concurrency.hpp"

namespace levitator{
namespace concurrency{

//So this works like a reference to an ostream which locks it exclusively until discarded
//And it also forwards the insertion operator
template<typename T, class Tr = std::char_traits<T>, class M = std::mutex>
class locked_ostream_ref;

template<typename T, class Tr, class M>
class locked_ostream_ref<std::basic_ostream<T, Tr>, M> : public locked_ref_base< std::basic_ostream<T, Tr>, M > {

    using base_type = locked_ref_base<std::basic_ostream<T, Tr>, M>;

public:
	using ostream_type = T;
    using base_type::base_type;

    locked_ostream_ref( const locked_ostream_ref &rhs ) = delete;
    locked_ostream_ref( locked_ostream_ref && ) = delete;
    locked_ostream_ref &operator=( const locked_ostream_ref & ) = delete;

    //general insertion
    template<typename T2>
    locked_ostream_ref &operator<<( T2 &&x){
        this->get() << std::forward<T2>(x);
        return *this;
    }

    //manipulator insertion
    //needed for overload resolution
    locked_ostream_ref &operator<<( ostream_type &(manip)(ostream_type &) ){
        this->get() << manip;
        return *this;
    }
};

//So this works like a reference to an ostream which locks it exclusively until discarded
//And it also forwards the insertion operator
template<typename T, class Tr = std::char_traits<T>, class M = std::mutex>
class locked_istream_ref;

template<typename T, class Tr, class M>
class locked_istream_ref<std::basic_istream<T, Tr>, M> : public locked_ref_base< std::basic_istream<T, Tr>, M > {

    using base_type = locked_ref_base<std::basic_istream<T, Tr>,M>;

public:    
    using base_type::base_type;

    locked_istream_ref( const locked_istream_ref &rhs ) = delete;
    locked_istream_ref( locked_istream_ref && ) = delete;
    locked_istream_ref &operator=( const locked_istream_ref & ) = delete;

    //general extraction
    template<typename T2>
    locked_istream_ref &operator>>( T2 &&x){
        this->get() >> std::forward<T2>(x);
        return *this;
    }
};

}
}
