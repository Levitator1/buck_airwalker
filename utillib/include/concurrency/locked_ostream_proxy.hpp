#pragma once

#include <functional>
#include <mutex>
#include <iostream>

namespace levitator{
namespace concurrency{

//An extension of std::reference_wrapper which implies lock/mutex ownership over its lifetime
template<typename T>
class locked_ref_base:public std::reference_wrapper<T>{

    using base_type = std::reference_wrapper<T>;
    std::lock_guard<M> m_guard;    

public:
    using value_type = T;
    using mutex_type = M;

protected:
    locked_proxy_base(value_type &obj, mutex_type &mutex):
        base_type(obj),
        m_guard(mutex){}
};

template<typename T, class M = std::mutex>
class locked_ref:public locked_ref_base<T,M>{
public:
    using locked_ref_base<T,M>::locked_ref_base;
};

//So this works like a reference to an ostream which locks it exclusively until discarded
//And it also forwards the insertion operator
template<typename T, class Tr = std::char_traits<T>, class M = std::mutex>
class locked_ostream_ref;

template<typename T, class Tr, class M>
class locked_ostream_ref<std::basic_ostream<T, Tr>, M> : public locked_ref_base< T, M > {

    using base_type = locked_ref_base<T,M>;

public:    
    using base_type::base_type;

    locked_ostream_proxy( const locked_ostream_proxy &rhs ) = delete;
    locked_ostream_proxy( locked_ostream_proxy && ) = delete;
    locked_ostream_proxy &operator=( const locked_ostream_proxy & ) = delete;

    //general insertion
    template<typename T2>
    locked_ostream_proxy &operator<<( T2 &&x){
        this->get(); << std::forward<T2>(x);
        return *this;
    }

    //manipulator insertion
    //needed for overload resolution
    locked_ostream_proxy &operator<<( ostream_type &(manip)(ostream_type &) ){
        this->get() << manip;
        return *this;
    }
};

}
}




