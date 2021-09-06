#pragma once
#include <algorithm>
#include <vector>

namespace jab{
namespace io{

/*
*
* A ringbuffer whose ranges are presented as pointers for ease of use in streambuf implementations
*
*/
template<typename T>
class ringbuffer{

public:
    using value_type = T;

private:
    ::size_t m_capacity, m_size;
    value_type *m_buffer, *m_buffer_end, *m_head, *m_tail;
    
public:
    ringbuffer( value_type *buf = nullptr, ::size_t sz = 1){
        buf(buf, sz);
    }
    
    void buf(value_type *buf, ::size_t sz){
        m_capacity = sz - 1;
        m_size = 0;
        m_tail = m_head = m_buffer = buf;
        m_buffer_end = m_buffer + sz;        
    }

    //Pull in n bytes having previously been written to the input range
    //We assume that nobody will ever try to write past the end of the range
    //So, we will check for them writing right up until the end to decide whether to wrap around.
    void push(::size_t n){
        m_tail += n;
        m_size += n;

        if(m_tail >= m_buffer_end)
            m_tail = m_buffer;
    }

    void pop(::size_t n){
        m_head += n;
        m_size -= n;

        if(m_head >= m_buffer_end)
            m_head = m_buffer;
    }

    //Insert one into the head end, analogous to unreading IO
    //It must then be popupated with an assignment to *put_begin()
    void unpop(){
        --m_head;
        if(m_head < m_buffer)
            m_head = m_buffer_end-1;
        ++m_size;
    }

    value_type *get_begin(){
        return m_head;
    }

    value_type *get_end(){
        return m_tail >= m_head ? m_tail : m_buffer_end;
    }

    value_type *put_begin(){
        return m_tail;
    }

    value_type *put_end(){
        return m_tail >= m_head ? m_tail : m_head - 1;
    }    
};

/*
template<typename T>
class ringbuf{
    std::vector<T> m_data;
    T *m_head, *m_tail;
    ::size_t m_size;

    const T *const_bufend() const{ return &m_data.back() + 1; }
    T *bufend(){ return const_cast<T *>(const_bufend());  }
    const T *bufend() const{ return const_bufend(); }
    
    void bump_ptr(T *&ptr, size_t ct){
        ptr += ct;
        if(ptr >= bufend())
            ptr = (&m_data.front()) + (ptr - bufend());
    }
    
public:
    ringbuf(::size_t cap){
        m_size = 0;
        m_data.resize(cap);
        m_head = m_tail = &m_data.front();
    }
 
    T *chunk_in_begin(){ return m_head; }
    const T *chunk_in_begin() const{ return m_head; }
    
    const T *const_chunk_in_end() const{
        return m_tail > m_head ? m_tail : bufend();
    }
    
    const T *chunk_in_end() const{ return const_chunk_in_end(); }
    T *chunk_in_end(){ return const_cast<T *>( const_chunk_in_end() );  }
    
    
    ::size_t chunk_in_size() const{ return chunk_in_end() - chunk_in_begin(); }
    
    const T *chunk_out_begin() const{ return m_tail; }
    
    const T *chunk_out_end() const{
        return m_tail < m_head ? m_head : bufend();
    }
    
    ::size_t chunk_out_size() const{ return chunk_out_end() - chunk_out_begin(); }
    
    void push(size_t c){
        bump_ptr(m_head, c);
        m_size += c;
    }
    
    void pop(size_t c){
        
        m_size -= c;
        
        //If the buffer is empty, reset the pointers for max chunk size
        if(!m_size)
            m_head = m_tail = &m_data.front();
        else
            bump_ptr(m_tail, c);
    }
    
    size_t size() const{ return m_size; }
};
*/
}
}
