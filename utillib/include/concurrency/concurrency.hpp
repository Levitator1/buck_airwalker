#pragma once

#include <mutex>
#include <memory>
#include <functional>

namespace levitator{
namespace concurrency{

//An extension of std::reference_wrapper which implies lock/mutex ownership over its lifetime
template<typename T, class M>
class locked_ref_base:public std::reference_wrapper<T>{

    using base_type = std::reference_wrapper<T>;
    std::lock_guard<M> m_guard;    

public:
    using value_type = T;
    using mutex_type = M;

    locked_ref_base(value_type &obj, mutex_type &mutex):
        base_type(obj),
        m_guard(mutex){}
};

template<typename T, class M = std::mutex>
class locked_ref:public locked_ref_base<T,M>{
public:
    using locked_ref_base<T,M>::locked_ref_base;
};

/*
namespace impl{
    
    //using locked_pointer_mutex_type = std::mutex;
    //using locked_pointer_lock_type = std::unique_lock<locked_pointer_mutex_type>;
    
    template<typename T>
    using locked_pointer_base = std::unique_ptr<T, levitator::functional::null_functor >;
}
    
template<typename T, class M = std::mutex, class L = std::unique_lock<M>>
class locked_pointer:public impl::locked_pointer_base<T>{
            
public:
    using value_type = T;
    using pointer_type = T *;
    using mutex_type = M;
    using lock_type = L;
    
private:
    lock_type m_lock;
        
public:    
    locked_pointer( mutex_type &mutex, T *obj):
        impl::locked_pointer_base<T>( obj, {} ),
        m_lock(mutex){
    }            
};
    
template<typename T>
class locked_container{
    //impl::locked_pointer_state<T> state;
    
public:
    //locked_container(T *obj):
    //    state{ {}, obj   }{}
    
    
        
};
*/

}}
