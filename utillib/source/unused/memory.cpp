#include <cstddef>
#include <algorithm>
#include "memory.hpp"

using namespace jab::util;

void jab::util::memory::copy( const memory &rhs ){

	if( m_state.shallow.capacity < rhs.m_state.shallow.size ){
		m_state.shallow = rhs.m_state.shallow;
		allocate();
	}else{
		m_state.shallow.size = rhs.m_state.shallow.size;
	}	
	std::copy( rhs.begin(), rhs.end(), m_state.aligned_buffer );
}

jab::util::memory::memory( const memory &rhs ){
	copy( rhs );
}

jab::util::memory::memory(size_type sz, size_type cap):
	m_state( { {sz, cap} } ){	
	allocate();
}

jab::util::memory &jab::util::memory::operator=( const memory &rhs  ){
	copy(rhs);
	return *this;
}

jab::util::memory &jab::util::memory::operator=( memory &&rhs  ){
	m_state = std::move(rhs.m_state);
	rhs.m_state.shallow.size = rhs.m_state.shallow.capacity = 0;
	return *this;
}

static char *new_buffer(jab::util::memory::size_type sz){
	return new char[ sz + alignof(std::max_align_t) ];
}

char *jab::util::memory::allocate(){
	auto buf = new_buffer(m_state.shallow.capacity);
	m_state.buffer = std::unique_ptr<char []>(buf);
	m_state.aligned_buffer = address<char>{ buf }.align();
	return buf;
}

jab::util::memory::size_type jab::util::memory::size() const{
	return m_state.shallow.size;
}

jab::util::memory::size_type jab::util::memory::capacity() const{
	return m_state.shallow.capacity;
}

void jab::util::memory::resize(size_type sz){

	if(sz > m_state.shallow.capacity){
		auto oldthis = std::move(*this);
		m_state.shallow.capacity = std::max(capacity() * 2, sz);
		m_state.shallow.size = sz;		
		allocate();
		std::copy( oldthis.begin(), oldthis.end(), m_state.aligned_buffer );
	}
	else	
		m_state.shallow.size = sz;
}

void jab::util::memory::push_back(size_type sz){
	resize( size() + sz );
}

char *jab::util::memory::data(){
	return m_state.aligned_buffer;
}

const char *jab::util::memory::data() const{
	return m_state.aligned_buffer;
}

char *jab::util::memory::begin(){
	return data();
}

const char *jab::util::memory::begin() const{
	return data();
}

char *jab::util::memory::end(){
	return data() + size();
}

const char *jab::util::memory::end() const{
	return data() + size();
}
