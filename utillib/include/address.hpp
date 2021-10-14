#pragma once
#include <cstdint>
#include <util.hpp>

namespace jab::util{

using int_address_type = std::uintptr_t;

template<typename T>
class address:public jab::util::delegate_arithmetic<int_address_type, address<T>>{
	using base_type = jab::util::delegate_arithmetic<int_address_type, address<T>>;
	using value_type = T;
	using pointer_type = value_type *;
	using int_type = int_address_type;

	int_type m_int;

public:

	//All the bits which represent misalignment. We assume 
	//alignments are always represented as a power of 2, with a single 1-bit
	static constexpr int_type align_mask = alignof(T) - 1;	

	static int_type to_int(pointer_type p){
		return reinterpret_cast<int_type>(p);
	}

	static pointer_type to_ptr(int_type x){
		return reinterpret_cast<pointer_type>(x);
	}

	address(pointer_type p = nullptr):
		base_type(m_int),
		m_int( to_int(p) ){}
	
	address(int_type x):
		base_type(m_int),
		m_int( x ){}

	int_type to_int() const{
		return m_int;
	}

	pointer_type to_ptr() const{
		return to_ptr(m_int);
	}

	operator pointer_type() const{
		return to_ptr();
	}

	//Calculate the positive offset needed to confrom this pointer to its referent's proper memory alignment
	int align_shift() const{
		return alignof(T) - (m_int & align_mask);
	}

	//Return a copy of this address having had align_shift() applied
	address align() const{
		return { m_int + align_shift() };
	}

};

}
