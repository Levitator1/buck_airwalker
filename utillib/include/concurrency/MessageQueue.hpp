#pragma once
#include <mutex>
#include <condition_variable>
#include <deque>

namespace levitator{
namespace concurrency{

template<class T>
class MessageQueue{

    using message_type = T;
    using container_type = std::deque<message_type *>;    
    std::mutex m_mutex;
    using lock_type = decltype( std::unique_lock(m_mutex)  );
    std::condition_variable m_cv;
    container_type m_messages;

    message_type *pop_impl(lock_type &lock){
        m_cv.wait(lock, [this](){ return !m_messages.empty(); } );
        auto result = m_messages.front();
        m_messages.pop_front();
        return result;
    }

public:
    void push( message_type *msg ){
        auto lock = std::unique_lock(m_mutex);
        m_messages.push_back(msg);
        m_cv.notify_all();
    }

    message_type *pop(){
        auto lock = std::unique_lock(m_mutex);
        return pop_impl(lock);
    }

    //Pop only if a message is immediately available
    //Returns nullptr for no messages
    //Cannot distinguish between a null message and no messages
    message_type *try_pop(){
        auto lock = std::unique_lock(m_mutex);
        if(m_messages.empty())
            return nullptr;
        else
            return m_messages.pop_impl(lock);
    }
};

}}
