#pragma once
#include <functional>
#include <algorithm>
#include <vector>
#include "util.hpp"

namespace jab{
namespace io{

/*
*
* A ringbuffer whose ranges are presented as pointers for ease of use in streambuf implementations
* There are up to two segments of content in a ringbuffer, so to see all of the contents, you must
* consume the first to roll the buffer over.
*
*/
template<typename T>
class ringbuffer{
public:
    using value_type = T;
	
private:
	struct State{
    	::size_t m_capacity, m_size;
    	value_type *m_buffer, *m_buffer_end, *m_head, *m_tail;
	} m_state;
    	
	auto mk_get_range(){
		return jab::util::range_property( [this](){ return this->get_begin(); }, [this](){ return this->get_end(); } );
	}

	auto mk_put_range(){
		return jab::util::range_property( [this](){ return this->put_begin(); }, [this](){ return this->put_end(); } );
	}

public:
	const decltype( std::declval<ringbuffer>().mk_get_range() ) get_range = this->mk_get_range();
	const decltype( std::declval<ringbuffer>().mk_put_range() ) put_range = this->mk_put_range();

    ringbuffer( value_type *bufp = nullptr, ::size_t sz = 0){
        buf(bufp, sz);
    }
    
	ringbuffer &operator=( ringbuffer &&rhs ){
		m_state = std::move(rhs.m_state);
		rhs.buf(nullptr, 0);
		return *this;
	}

    ::size_t size() const{
        return m_state.m_size;
    }

    ::size_t capacity() const{
        return m_state.m_capacity;
    }

    void buf(value_type *buf, ::size_t sz){
        m_state.m_capacity = sz;
        m_state.m_size = 0;
        m_state.m_tail = m_state.m_head = m_state.m_buffer = buf;
        m_state.m_buffer_end = m_state.m_buffer + sz;        
    }

    //Pull in n bytes having previously been written to the input range
    //We assume that nobody will ever try to write past the end of the range
    //So, we will check for them writing right up until the end to decide whether to wrap around.
    void push(::size_t n){

		if(!n) return;
		if(m_state.m_tail == m_state.m_buffer_end)
            m_state.m_tail = m_state.m_buffer;

        m_state.m_tail += n;
        m_state.m_size += n;        
    }

    void pop(::size_t n){
        m_state.m_head += n;
        m_state.m_size -= n;

        if(m_state.m_size == 0)
            m_state.m_head = m_state.m_tail = m_state.m_buffer;
        else if(m_state.m_head >= m_state.m_buffer_end)
            m_state.m_head = m_state.m_buffer;
    }

    //Insert one into the head end, analogous to unreading IO
    //It must then be upated with an assignment to *put_begin()
    void unpop(){
        --m_state.m_head;
        if(m_state.m_head < m_state.m_buffer)
            m_state.m_head = m_state.m_buffer_end - 1;
        ++m_state.m_size;
    }

    value_type *get_begin() const{
        return m_state.m_head;
    }

    value_type *get_end() const{
        return m_state.m_tail >= m_state.m_head ? m_state.m_tail : m_state.m_buffer_end;
    }

    value_type *put_begin() const{
        return m_state.m_tail;
    }

    value_type *put_end() const{
        return m_state.m_tail >= m_state.m_head ? m_state.m_tail : m_state.m_head;
    }

    value_type *get_begin(){
        return m_state.m_head;
    }

    value_type *get_end(){
        return m_state.m_tail >= m_state.m_head ? m_state.m_tail : m_state.m_buffer_end;
    }

    value_type *put_begin(){
		return m_state.m_tail == m_state.m_buffer_end ? m_state.m_buffer : m_state.m_tail;
    }

    value_type *put_end(){
        return m_state.m_tail >= m_state.m_head ? m_state.m_tail : m_state.m_head;
    }

    //How many elements are available to fetch in the current segment, not total
    ::size_t getavail() const{
        return get_end() - get_begin();
    }

    ::size_t putavail() const{
        return put_end() - put_begin();
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
