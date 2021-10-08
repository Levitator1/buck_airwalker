#pragma once
#include <mutex>
#include <condition_variable>
#include <deque>
#include "meta.hpp"

namespace levitator{
namespace concurrency{

template<class T>
class MessageQueue{

    using message_type = T;
    using container_type = std::deque<message_type>;
	using mutex_type = std::mutex;

    mutex_type m_mutex;
    using lock_type = decltype( std::unique_lock(m_mutex)  );
    std::condition_variable m_cv;
    container_type m_messages;

    message_type pop_impl(lock_type &lock){
        m_cv.wait(lock, [this](){ return !m_messages.empty(); } );
        auto result = std::move(m_messages.front());
        m_messages.pop_front();
        return result;
    }

	class MutateGuard{
		MessageQueue &m_queue;
		lock_type m_lock;

	public:
		MutateGuard(MessageQueue &q):
			m_queue(q),
			m_lock(q.m_mutex){}
		
		~MutateGuard(){
			m_queue.m_cv.notify_all();
		}
	};

public:

	template<typename U, typename = jab::meta::permit_gl_ref<U, message_type> >
    void push_back( U &&msg){
        MutateGuard(*this);
        m_messages.push_back( std::forward<U>(msg));        
    }
	
	template<typename U, typename = jab::meta::permit_gl_ref<U, message_type> >
	void push_front( U &&msg ){
        MutateGuard(*this);
        m_messages.push_front( std::forward<U>(msg));        
    }

    message_type pop(){
        auto lock = std::unique_lock(m_mutex);
        return pop_impl(lock);
    }

    //Pop only if a message is immediately available
    //Returns nullptr for no messages
    //Cannot distinguish between a null message and no messages
    message_type try_pop(){
        auto lock = std::unique_lock(m_mutex);
        if(m_messages.empty())
            return nullptr;
        else
            return m_messages.pop_impl(lock);
    }
};

}}
