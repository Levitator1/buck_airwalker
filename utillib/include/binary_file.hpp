#pragma once
#include <type_traits>
#include <mutex>
#include <iostream>
#include "util.hpp"
#include "concurrency/concurrency.hpp"
#include "File.hpp"

namespace levitator::util::binfile{

//File position data to be written directly to the file as standard-layout-type
//It's just a wrapper around an offset, but it conveys type information for type safety
template<typename T>
class FilePosition{
	std::streamoff m_off;

public:	
	using value_type = T;	

	FilePosition(std::streamoff off = 0):
		m_off(off){}

	std::streamoff offset() const{
		return m_off;
	}

	operator bool() const{
		return m_off != 0;
	}
};

template<typename T>
class BinaryFilePtr;

//Just kind of formalizes the idea that we're dealing with a binary file
//and allows it to be mutex-locked for concurrency
class BinaryFile{
	using mutex_type = std::recursive_mutex;
	using file_ref_type = concurrency::locked_ref<std::iostream, mutex_type>;

	mutex_type m_mutex;
	std::iostream &m_file;

public:

	template<typename T>
	using ptr = BinaryFilePtr<T>;

	BinaryFile( std::iostream &file );
	~BinaryFile();
	file_ref_type get();

	template<typename T>
	ptr<T> fetch( std::streamoff off ){
		return { *this, off };
	}

	template<typename T>
	ptr<T> fetch( const FilePosition<T> &pos ){
		return fetch<T>( pos.offset() );
	}

	//lazy store
	template<typename T>
	ptr<T> store( std::streamoff off, T &&data ){
		return { *this, off, std::move(data) };
	}

	template<typename T>
	ptr<T> store( const FilePosition<T> &pos, T &&data ){
		return store<T>( pos.offset(), std::move(data) );
	}

	std::streampos end_offset(){
		auto lock = std::lock_guard(m_mutex);
		m_file.seekp(0 , std::ios_base::end);
		return m_file.tellp();
	}

	template<typename T>
	ptr<T> append( T &&data ){
		auto lock = std::lock_guard(m_mutex);
		auto result = store( end_offset(), std::move(data) );
		result.store();	//write now so that other appends won't end up in the same position
		return result;
	}
};

//Pointer into a binary file that maintains a cache of the standard-layout object found there
//and remembers to write it if a non-const view is retrieved.
//Only as lightweight as the cached object, so don't pass it around trivially. It's a heavy(?) pointer.
//This could be fixed with heap allocation, but then that's a tradeoff, too.
//
//Warning: This assumes that pointer instances never refer to duplicate or overlapping regions
// this is actually kind of a fatal stipulation which will probably prevent me from using this
// at all
template<typename T>
class BinaryFilePtr {

public:
	using value_type = T;

private:
	template<typename U>
	struct cache_box{
		mutable U cache;
	};

	template<typename U>
	struct cache_box<const U>{
		const U cache;
	};

	BinaryFile &m_file;
	bool m_dirty;
	std::streamoff m_offset;
	cache_box<T> m_cache;	//TODO: See what happens if it's mutable const

	BinaryFilePtr( BinaryFile &file, std::streamoff o, value_type &&val, jab::util::null_type ):
		m_file(file),		
		m_offset(o),
		m_cache( { std::move(val) } ){

		//Enable exceptions on IO error
		auto fref = file.get();
		fref.get().exceptions( std::iostream::badbit | std::iostream::failbit | std::iostream::eofbit );		
	}

public:
	static_assert( std::is_standard_layout_v<value_type>, "BinaryFilePtr can only point to standard-layout types");

	BinaryFilePtr( BinaryFile &file, std::streamoff o):
		BinaryFilePtr( file, o, {}, jab::util::null_type{} ){

		m_dirty = false;
		load();
	}

	BinaryFilePtr( BinaryFile &file, std::streamoff o, value_type &&val):
		BinaryFilePtr( file, o, std::move(val), jab::util::null_type{} ){
		
		m_dirty = true;
	}

	~BinaryFilePtr(){
		if(m_dirty)
			store();
	}
	
	void load(){
		auto fref = m_file.get();
		fref.get().seekg(m_offset);
		m_dirty = false;
		fref.get().read( (char *)&m_cache, sizeof(m_cache) );		
	}

	void store(){
		auto fref = m_file.get();
		fref.get().seekp(m_offset);
		fref.get().write( (const char *)&m_cache, sizeof(m_cache) );
		m_dirty = false;
	}

	T &get() const{
		m_dirty = !std::is_const_v<value_type>;
		return m_cache;
	}

	const T &get_const() const{
		return m_cache;
	}

	T *operator->() const{		
		return &get();
	}

	T &operator*() const{
		return get();
	}
};

namespace blocks{
	template<typename T>
	struct linked_list;
}

template<typename T>
class binary_file_linked_list_iterator{
	//BinaryFilePtr m_ptr;
};

//Standard-layout chunks that can be stored directly in a binary file
namespace blocks{

//A relative pointer which is inherently relocatable so that
//if the joint offset of itself and its referent changes, it
//still points correctly.
template<typename T>
class RelPtr{
public:
	using value_type = T;
	using ref_type = T &;
	using pointer_type = T *;

	//Takes the pointer type, not T
	template<typename P>
	using obj_char_ptr_type = jab::util::copy_cv<P, char *>;	

private:	
	::ssize_t m_offset;

	template<typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	using this_char_ptr_type = jab::util::copy_cv<This, char *>;

	template<typename U, typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	inline static ::ssize_t make_offset( U *obj, This *th){		
		auto lhs = jab::util::static_const_cast<obj_char_ptr_type<U *>>(obj);
		auto rhs = jab::util::static_const_cast< this_char_ptr_type<This> >(th);
		return lhs - rhs;
	}

	template<typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	static pointer_type make_pointer( This *th, ::size_t offs){		
		auto thisptr = jab::util::static_const_cast< this_char_ptr_type<This> >(th);
		return jab::util::static_const_cast< pointer_type >( thisptr + offs );
	}

public:
	inline RelPtr(T *ptr):
		m_offset( make_offset(ptr, this) ){}

	inline RelPtr(const T &rhs):
		m_offset( make_offset( rhs.operator->(), this ) ){}

	//Move is the same as copying
	inline RelPtr(T &&rhs):
		RelPtr( rhs  ){}

	inline ::ssize_t offset() const{
		return m_offset;
	}

	//Offset relative to some fixed address, probably usually the start of the file
	template<typename U>
	inline ::ssize_t offset(U *base) const{
		auto thisoffs = make_offset(base, this) * -1;
		return thisoffs + m_offset;
	}

	inline RelPtr &operator=( const RelPtr &rhs ){
		m_offset = make_offset( rhs.operator->(), this );
		return *this;
	}

	inline RelPtr &operator=( RelPtr &&rhs){
		return operator=(rhs);
	}

	inline pointer_type operator->(){
		return make_pointer(this, m_offset);
	}

	inline pointer_type operator->() const{
		return make_pointer(this, m_offset);
	}

	inline ref_type operator*(){
		return *operator->();
	}

	inline ref_type operator*() const{
		return *operator->();
	}
};

template<typename T>
struct linked_list{
	template<typename U>
	using file_pos = FilePosition<U>;

	using value_type = T;
	file_pos<value_type> body;
	file_pos<linked_list<value_type>> next;	
};

}
}
