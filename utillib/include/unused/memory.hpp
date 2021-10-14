#pragma once
#include <memory>
#include "address.hpp"
#include "memory.hpp"

//A class that manages a resizable, linear, maximally aligned char buffer intended to store
//binary data, such as binary file images.
//std::vector doesn't cut it for memory management because it has no concept of aligning a char buffer
namespace jab::util{

class memory{
public:
	using size_type = std::size_t;

private:
	struct State{
		struct Shallow{
			size_type size = 0, capacity = 0;
		} shallow;

		std::unique_ptr<char []> buffer = {nullptr};		
		address<char> aligned_buffer = {nullptr};
	} m_state;

	void copy( const memory & );
	char *allocate();

public:
	memory(const memory &);
	memory(memory &&);
	memory(size_type sz, size_type cap);
	//memory(size_type sz, size_type cap, const char *source=nullptr );

	memory &operator=( memory &&);
	memory &operator=( const memory &);

	void resize(size_type sz);
	void push_back(size_type sz);

	size_type capacity() const;
	size_type size() const;
	char *data();
	const char *data() const;
	char *begin();
	const char *begin() const;
	char *end();
	const char *end() const;
};
}
