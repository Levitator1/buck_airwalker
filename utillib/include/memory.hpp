#pragma once

#include <cstdlib>
#include <memory>

namespace jab::util {

//Allocate a char buffer aligned for AlignT. Standard C++ applies strict no-aliasing rules,
//but it's ok to alias as a char buffer for binary representations. So, this is always a char allocator.
//But it's aligned for AlignT. If you don't know in advance what to align for, you can specify std::max_align,
//and get a buffer aligned for anything.
//
//I guess it's part of the allocator's interface that the first template argument is the value_type,
//so we will keep that convention even though we are intended to always be char.
template<typename T, typename AlignT = std::max_align_t>
class aligned_binary_allocator:public std::allocator<char>{
	using base_type = std::allocator<T>;	

public:
	using align_type = AlignT;
	using value_type = typename base_type::value_type;

	aligned_binary_allocator() = default;

	template<typename U, typename V = align_type>
	using rebind_alloc = aligned_binary_allocator<U,V>;

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
