#pragma once
#include <cstddef>
#include <type_traits>
#include <mutex>
#include <iostream>
#include <vector>
#include "util.hpp"
#include "meta.hpp"
#include "address.hpp"
#include "memory.hpp"
#include "concurrency/concurrency.hpp"
#include "File.hpp"

namespace levitator::binfile{

namespace blocks{
	template<typename T>
	struct linked_list;

	template<typename T, class Traits>
	class RelPtr;

	template<typename T, typename Base, class Traits>
	class OffsetPtr;
}

//Just kind of formalizes the idea that we're dealing with a binary file
//and allows it to be mutex-locked for concurrency
class BinaryFile{
public:
	using mutex_type = std::recursive_mutex;
	using lock_type = std::lock_guard<mutex_type>;

	//Allow scoped lock for this file to be attached to a reference to something else
	template<typename T = BinaryFile>
	using locked_ref = concurrency::locked_ref<T, mutex_type>;

	template<typename T = BinaryFile>
	using const_locked_ref = concurrency::locked_ref<const T, mutex_type>;

private:	
	using file_ref_type = locked_ref<std::iostream>;	

	mutable mutex_type m_mutex;
	std::iostream &m_file;
	std::vector<char, jab::util::aligned_binary_allocator<std::max_align_t>> m_cache;	
	
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

	//A scoped, nestable guard which rolls back all subsequent appends if it goes out of scope
	//without release() having been called.
	class AppendGuard{
		BinaryFile *m_bf;	
		locked_ref<BinaryFile> m_lock;
		std::streamoff m_position;
		

	public:
		AppendGuard(BinaryFile &file);
		~AppendGuard();
		void release();
	};

public:
	using size_type = typename decltype(m_cache)::size_type;

	BinaryFile();
	BinaryFile( std::iostream &file, std::streamsize initial_capacity = 0 );
	~BinaryFile();	

	//Commits the memory image to disk and flushes the I/O buffers
	void flush();

	//If ref is relocatable make sure to lock before you retrieve ref!
	//Then call make_lock() to make the locked reference after you have already secured ref.
	template<typename U>
	locked_ref<U> make_lock(U &ref) const{
		return {ref, m_mutex};		
	}
	
	locked_ref<const BinaryFile> make_lock() const{
		return make_lock(*this);		
	}

	locked_ref<> make_lock(){
		return make_lock(*this);		
	}

	locked_ref<std::iostream> get_stream() const{
		return make_lock(m_file);
	}

	//file_ref_type get(); //Return the file stream wrapped in a reference holding a scoped-lock
	std::streamsize size_on_disk() const;
	size_type size() const;

	//Not implementing any concept of a free/reuse store, so there is no free, only allocation/append
	template<typename T>
	auto alloc(T &&obj){
		using result_type = std::remove_reference<T>::type;

		//Bump size for alignment if necessary
		auto addr = jab::util::address(m_cache.data());
		std::size_t sz;

		sz = sizeof(result_type);
		if(addr){			
			sz += addr.align_shift();
		}				

		auto lock = make_lock();
		m_cache.resize( m_cache.size() + sz);
		auto ptr = (&*m_cache.end()) - sizeof(result_type);
		auto ptr2 = jab::util::reinterpret_const_cast<result_type *>( ptr );
		return ptr2;
	}

	//resize the file to a length of n bytes
	void resize(std::streamsize n){
		auto lock = make_lock();
		m_cache.resize(n);
	}

	//Shrink the file by n bytes. Closest thing to a concept of freeing.
	void pop_back(std::streamsize n){
		auto lock = make_lock();	
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

	template<typename T, typename Ptr>
	T *list_insert( Ptr &ptr, T &&obj){
		AppendGuard guard(*this);
		auto objp = alloc<T>( std::forward<T>(obj) );
		list_insert( ptr, objp);
		guard.release();
		return objp;
	}

	template<typename T, typename Ptr>
	T *list_insert( Ptr &ptr, T *objp){		
		auto linkp = alloc( binfile::blocks::linked_list<T>{objp, ptr} );
		ptr = linkp;
		return objp;
	}
};

//T is the type the list is declared with
//U is T with optional CV qualifiers
//Can maintain an optional scoped lock associated with the scope of the iterator
//to prevent the iterated list from updating.
template<typename T, typename U, class LockType = jab::util::null_type>
class linked_list_iterator{
public:
using value_type = U;
	using list_type = blocks::linked_list<T>;
	using lock_type = LockType;
	lock_type m_lock;
	const list_type *m_linkp;

public:

	//Seems to be called "rebinding" to reparameterize an existing template instance
	template<typename LT>
	using rebind_for_lock = linked_list_iterator<T, U, LT>;

	linked_list_iterator(const list_type *linkp = nullptr, lock_type &&lock = {}):
		m_lock( std::move(lock)),
		m_linkp(linkp){}

	
	linked_list_iterator &lock( lock_type &&lk ){
		m_lock = std::move(lk);
		return *this;
	}

	//Let's get weird.  Returns a copy of the same iterator, as a slightly different type, now with the
	//provided scoped lock embedded.
	template<typename L>
	auto lock( L &&lk ){
		using new_lock_type = std::remove_reference_t<L>;
		return linked_list_iterator<T, U, new_lock_type>{m_linkp, std::move(lk)};
	}

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

//A traits type which defines overloads of base_ptr(), which take some variety of RelPtr
//and return a base address for the pointer object to be relative to. The default is a pointer
//to the pointer object itself, so that it can relocatable and relative to its own location.
//T: type of referent
//For base_ptr(U), U is: some variety of pointer which will internally call base_ptr to find out what its base address is
template<typename T>
struct RelPtr_traits{
		
	using RelPtr_type = binfile::blocks::RelPtr<T, RelPtr_traits>;

	static auto base_ptr( const RelPtr_type &ptr ){
		return &ptr;
	}

	static auto base_ptr( RelPtr_type &ptr ){
		return &ptr;
	}

};

//Standard-layout chunks that can be stored directly in a binary file
namespace blocks{

//A relative pointer which is inherently relocatable so that
//if the joint offset of itself and its referent changes, it
//still points correctly. Could be thought of as self-relative offset pointer.
template<typename T, typename Traits = RelPtr_traits<T>>
class RelPtr{
public:
	using value_type = T;
	using traits_type = Traits;
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
	inline static ::ssize_t make_offset( U *obj, This *thisp){
		using this_type = typename jab::util::copy_cv<This, typename traits_type::RelPtr_type>::type;

		auto thisp2 = static_cast<this_type *>(thisp);
		return to_byte_ptr(obj) - to_byte_ptr( traits_type::base_ptr( *thisp2 ) );
	}

	//template<typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	//static pointer_type make_pointer( This *th, ::size_t offs){
	//	return jab::util:static_const_cast<pointer_type>( static_cast<void *>(to_byte_ptr(th) + offs)  );
	//}

	template< typename This, typename = jab::meta::permit_any_cv<This, RelPtr> >
	inline static pointer_type make_pointer(This *thisp){
		using voidp_type = typename jab::util::copy_cv<This, void>::type *;
		using this_type = typename jab::util::copy_cv<This, typename traits_type::RelPtr_type>::type;

		auto thisp2 = static_cast<this_type *>(thisp);
		return jab::util::reinterpret_const_cast<pointer_type>( reinterpret_cast<voidp_type>(to_byte_ptr( traits_type::base_ptr(*thisp2) ) + thisp->offset()) );
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

template<typename T, typename BaseF>
struct OffsetPtr_traits{
	using RelPtr_type = OffsetPtr<T, BaseF, OffsetPtr_traits>;

	static auto base_ptr( const RelPtr_type &ptr){
		return ptr.base_ptr();
	}
};

//An elaboration on RelPtr which calls a function to retrieve its base address.
//This is so that you can retrieve the base address from the contents of a vector
//which may reallocate its buffer, for example.
template<typename T, class BaseF, class Traits = OffsetPtr_traits<T, BaseF>>
class OffsetPtr:public RelPtr<T, Traits>{
public:
	using base_type = RelPtr<T, Traits>;	//base class
	using base_ptr_function = BaseF;
	using value_type = typename base_type::value_type;
	using pointer_type = typename base_type::pointer_type;

private:
	base_ptr_function m_base_f;

public:
	OffsetPtr(pointer_type pobj, base_ptr_function func):
		base_type(pobj),		
		m_base_f(func){}	

	auto base_ptr() const{
		return m_base_f();
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
