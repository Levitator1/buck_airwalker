#pragma once
#include <type_traits>
#include <mutex>
#include <iostream>
#include <vector>
#include "util.hpp"
#include "meta.hpp"
#include "concurrency/concurrency.hpp"
#include "File.hpp"

namespace levitator::binfile{

//Just kind of formalizes the idea that we're dealing with a binary file
//and allows it to be mutex-locked for concurrency
class BinaryFile{
	using mutex_type = std::recursive_mutex;
	using lock_type = std::lock_guard<mutex_type>;
	using file_ref_type = concurrency::locked_ref<std::iostream, mutex_type>;

	mutable mutex_type m_mutex;
	std::iostream &m_file;
	std::vector<char> m_cache;	
	
	//Assumes mutex lock is in place
	template<typename T, typename This>
	static auto do_fetch(This *thisp, std::streampos pos){
		using obj_ptr_type = jab::util::copy_cv<This, T>::type *;
		auto guard = std::lock_guard(thisp->m_mutex);
		auto ptr = &thisp->m_cache.front() + pos;
		return jab::util::reinterpret_const_cast<obj_ptr_type>( ptr );
	}	

	//Not really logically or effectively const, so don't use it that way
	std::streamsize size_on_disk_impl() const; //seeks to the end of the file as a side-effect

public:
	using size_type = typename decltype(m_cache)::size_type;

	BinaryFile( std::iostream &file, std::streamsize initial_capacity = 0 );
	~BinaryFile();

	//Commits the memory image to disk and flushes the I/O buffers
	void flush();

	lock_type make_lock() const{
		return lock_type{ m_mutex };
	}

	file_ref_type get();
	std::streamsize size_on_disk() const;
	size_type size() const;

	//Not implementing any concept of a free/reuse store, so there is no free, only allocation/append
	template<typename T, typename... Args>
	T *alloc(Args &&... args){
		auto lock = make_lock();
		m_cache.resize(sizeof(T));
		auto ptr = &m_cache.back() - sizeof(T) + 1;
		auto ptr2 = jab::util::reinterpret_const_cast<T *>( ptr );
		new (ptr2) T( std::forward<Args>(args)... );
		return ptr2;
	}

	//resize the file to a length of n bytes
	void resize(std::streamsize n){
		auto lock = make_lock();
		m_cache.resize(n);
	}

	//Shrink the file by n bytes. Closest thing to a concept of freeing.
	void pop_back(std::streamsize n){		
		resize( size() - n );
	}

	template<typename T>
	T *fetch(std::streampos pos){
		auto lock = make_lock();
		return do_fetch<T>(this, pos);
	}

	template<typename T>
	const T *fetch(std::streampos pos) const{
		auto lock = make_lock();
		return do_fetch<T>(this, pos);
	}
};

//A scoped, nestable guard which rolls back all subsequent appends if it goes out of scope
//without release() having been called.
class AppendGuard{
	BinaryFile *m_bf;
	std::streamoff m_position;

public:
	AppendGuard(BinaryFile &file);
	~AppendGuard();
	void release();
};

namespace blocks{
	template<typename T>
	struct linked_list;

	template<typename T>
	class RelPtr;
}

//T is the type the list is declared with
//U is T with optional CV qualifiers
template<typename T, typename U>
class linked_list_iterator{
public:
using value_type = U;
	using list_type = blocks::linked_list<T>;	
	const list_type *m_linkp;

public:
	linked_list_iterator(const list_type *linkp = nullptr):
		m_linkp(linkp){}
	
	value_type &operator*() const{
		return *m_linkp->value_ptr;
	}

	value_type *operator->() const{
		return *m_linkp->value_ptr;
	}

	linked_list_iterator &operator++(){
		m_linkp = &*m_linkp->next;
		return *this;
	}

	bool operator==(const linked_list_iterator &rhs){
		return m_linkp == rhs.m_linkp;
	}

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
	//template<typename P>
	//using obj_char_ptr_type = typename jab::util::copy_cv<P, char *>::type;	

private:	
	::ssize_t m_offset;

	//template<typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	//using this_char_ptr_type = typename jab::util::copy_cv<This, char *>::type;

	//Convert an arbitrary pointer to a char pointer of the same cv type;
	//a pointer to its byte representation
	template<typename U>
	static auto to_byte_ptr(U *objp){
		using voidp_type = typename jab::util::copy_cv<U, void>::type *;
		using charp_type = typename jab::util::copy_cv<U, char>::type *;
		return static_cast<charp_type>(reinterpret_cast<voidp_type>(objp));
	}

	template<typename U, typename This, typename = jab::meta::permit_any_cv<This, RelPtr> >
	inline static ::ssize_t make_offset( U *obj, This *th){		
		return to_byte_ptr(obj) - to_byte_ptr(th);
	}

	//template<typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	//static pointer_type make_pointer( This *th, ::size_t offs){
	//	return jab::util:static_const_cast<pointer_type>( static_cast<void *>(to_byte_ptr(th) + offs)  );
	//}

	template< typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	inline static pointer_type make_pointer(This *thisp){
		using voidp_type = typename jab::util::copy_cv<This, void>::type *;
		return jab::util::reinterpret_const_cast<pointer_type>( reinterpret_cast<voidp_type>(to_byte_ptr( thisp ) + thisp->offset()) );
	}

	//Handle the special case where a null pointer is represented via zero-offset or self-reference
	inline ::size_t copy( const RelPtr &rhs ){
		if(rhs.offset() == 0)
			return 0;
		else
			return make_offset( rhs.operator->(), this );
	}

public:
	//inline RelPtr(T *ptr = this):	//This doesn't work, unsurprisingly
	inline RelPtr(T *ptr = nullptr):
		m_offset( ptr ? make_offset(ptr, this) : 0 ){}

	inline RelPtr(T &obj):
		RelPtr(&obj){
	}

	inline RelPtr(const RelPtr &rhs):
		m_offset( copy(rhs) ){}

	//Move is the same as copying
	inline RelPtr(T &&rhs):
		RelPtr( rhs  ){}

	inline ::ssize_t offset() const{
		return m_offset;
	}

	inline operator bool() const{
		return m_offset != 0;
	}

	//Offset relative to some fixed address, probably usually the start of the file
	template<typename U>
	inline ::ssize_t offset(U *base) const{
		auto thisoffs = make_offset(base, this) * -1;
		return thisoffs + m_offset;
	}

	inline RelPtr &operator=( const RelPtr &rhs ){
		m_offset = copy(rhs);
		return *this;
	}

	inline RelPtr &operator=( RelPtr &&rhs){
		return operator=(rhs);
	}

	inline pointer_type operator->(){
		return make_pointer(this);
	}

	inline pointer_type operator->() const{
			return make_pointer(this);
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
	using value_type = T;
	using iterator_type = linked_list_iterator<T, T>;
	using const_iterator_type = linked_list_iterator<T, const T>;
	RelPtr<value_type> value_ptr;
	RelPtr<linked_list> next;
	
public:
	linked_list( const RelPtr<value_type> &val, const RelPtr<linked_list> &nx ):
		value_ptr(val),
		next(nx){}

	template<typename P>
	static void push_front(P &ptr, linked_list &link){		
		link.next = ptr;
		ptr = &link;
	}

	static const_iterator_type begin( const linked_list *ptr ){
		return {ptr};
	}

	static iterator_type begin( linked_list *ptr ){
		return {ptr};
	}

	static const_iterator_type end( const linked_list *ptr ){
		return {nullptr};
	}

	static iterator_type end( linked_list *ptr ){
		return {nullptr};
	}

	static const_iterator_type cbegin( const linked_list *ptr ){
		return {ptr};
	}

	static const_iterator_type cend( const linked_list *ptr ){
		return {nullptr};
	}
	
	auto begin() const{
		return begin(this);
	}

	auto begin(){
		return begin(this);
	}
	
	auto end() const{
		return end(this);
	}

	auto end(){
		return end(this);
	}

	auto cbegin() const{
		return begin(this);
	}

	auto cend() const{
		return end(this);
	}
};

}
}
