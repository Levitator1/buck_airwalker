#pragma once

#include <cstdlib>
#include <memory>

namespace jab::util {

//Allocate a char buffer aligned for T. Standard C++ applies strict no-aliasing rules,
//but it's ok to alias as a char buffer for binary representations. So, this is always a char allocator.
//But it's aligned for T. If you don't know in advance what to align for, you can specify std::max_align,
//and get a buffer aligned for anything.
template<typename T>
class aligned_binary_allocator:public std::allocator<char>{
	using base_type = std::allocator<char>;

public:
	using align_type = T;
	using value_type = typename base_type::value_type;
	using base_type::base_type;	

	//C++ 20
	constexpr value_type* allocate( std::size_t n ){
		return static_cast<value_type *>(std::aligned_alloc( alignof(align_type), n * sizeof(value_type) ));
	}

	//Pre-C++ 20
	constexpr value_type* allocate( std::size_t n, const void *hint ){
		return allocate(n);
	}

	constexpr void deallocate( value_type* p, std::size_t n ){
		std::free(p);
	}
};

}
