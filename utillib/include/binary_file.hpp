#pragma once
#include <cstddef>
#include <memory>
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

template<typename T>
class BinaryFile_allocator;

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

	template<typename T>
	using allocator = BinaryFile_allocator<T>;

private:	
	using file_ref_type = locked_ref<std::iostream>;	

	mutable mutex_type mutex;

	struct State{		
		std::iostream *file;
		std::vector<char, jab::util::aligned_binary_allocator<char, std::max_align_t>> cache;
	} m_state;
	
	//Assumes mutex lock is in place
	template<typename T, typename This>
	static auto do_fetch(This *thisp, std::streampos pos){
		using obj_ptr_type = typename jab::util::copy_cv<This, T>::type *;
		auto guard = std::lock_guard(thisp->mutex);
		auto ptr = &thisp->m_state.cache.front() + pos;
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
	using size_type = typename decltype(m_state.cache)::size_type;

	BinaryFile();
	BinaryFile( std::iostream &file, std::streamsize initial_capacity = 0 );
	~BinaryFile();

	BinaryFile &operator=( BinaryFile &&rhs );

	//Commits the memory image to disk and flushes the I/O buffers
	void flush();

	operator bool() const{
		return m_state.file;
	}

	//If ref is relocatable make sure to lock before you retrieve ref!
	//Then call make_lock() to make the locked reference after you have already secured ref.
	template<typename U>
	locked_ref<U> make_lock(U &ref) const{
		return {ref, mutex};		
	}
	
	locked_ref<const BinaryFile> make_lock() const{
		return make_lock(*this);		
	}

	locked_ref<> make_lock(){
		return make_lock(*this);		
	}

	locked_ref<std::iostream> get_stream() const{
		return make_lock(*m_state.file);
	}

	//file_ref_type get(); //Return the file stream wrapped in a reference holding a scoped-lock
	std::streamsize size_on_disk() const;
	size_type size() const;

	//allocate a specific size with alignment
	void *allocate(std::size_t sz, int align);

	template<typename T>
	T *allocate(std::size_t n){
		using result_type = typename std::remove_reference<T>::type;
		return reinterpret_cast<result_type *>( allocate(sizeof(result_type) * n, alignof(result_type)) );
	}

	//Not implementing any concept of a free/reuse store, so there is no free, only allocation/append
	template<typename T, typename... Args>
	auto construct( Args... args ){						
		return new( allocate<T>(1) ) T( std::forward<Args>(args)... );
	}

	//resize the file to a length of n bytes
	void resize(std::streamsize n){
		auto lock = make_lock();
		m_state.cache.resize(n);
	}

	//Shrink the file by n bytes. Closest thing to a concept of freeing.
	void pop_back(std::streamsize n){
		auto lock = make_lock();	
		resize( size() - n );
	}

	void file( std::iostream &stream );

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

	/*
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
	*/
};

template<typename T>
class BinaryFile_allocator{
	BinaryFile *m_file;

public:
	using value_type = T;
	using pointer = value_type *;

	template<typename U>
	using rebind = BinaryFile_allocator<U>;

	//It's a standard convention to be able to rebind an allocator, so they need
	//to be constructible from each other for this to be fully useful
	template<typename U>
	BinaryFile_allocator(const BinaryFile_allocator<U> &other):
		m_file(other.m_file){}

	//Default-constructed invalid allocator
	BinaryFile_allocator():
		m_file(nullptr){}

	BinaryFile_allocator(BinaryFile &file):
		m_file(&file){}

	pointer allocate(std::size_t n){
		return m_file->allocate<T>(n);
	}
};

//T is link_type::value_type possibly with additional CV qualifiers
//Can maintain an optional scoped lock associated with the scope of the iterator
//to prevent the iterated list from updating.
template<typename T, typename LinkP, class LockType = jab::util::null_type>
requires jab::util::is_dereferenceable<LinkP>
class linked_list_iterator{
public:
	using link_pointer = LinkP;
	using link_type = typename std::pointer_traits<link_pointer>::element_type;
	using value_type = T;
	//using list_type = blocks::linked_list<T>;
	using lock_type = LockType;

	//Seems to be called "rebinding" to reparameterize an existing template instance
	template<typename U, typename LinkP2 = LinkP, class LockType2 = LockType>
	using rebind = linked_list_iterator<U, LinkP2, LockType2>;

	lock_type m_lock;
	link_pointer m_linkp;

/*
protected:
	template<typename U, typename LinkP2, class LockType2 >
	auto lock_rebound_impl( LockType2 &&lk ){
		using LockType3 = std::remove_reference_t<LockType2>;
		using new_lock_type = std::remove_reference_t<L>;
		return This<LinkP, new_lock_type>{m_linkp, std::move(lk)};
	}
*/

public:
	linked_list_iterator(const link_pointer &linkp = {nullptr}, lock_type &&lock = {}):
		m_lock( std::move(lock)),
		m_linkp(linkp){}

	
	/*
	linked_list_iterator &lock( lock_type &&lk ){
		m_lock = std::move(lk);
		return *this;
	}
	*/

	//Let's get weird.  Returns a copy of the same iterator, as a slightly different type, now with the
	//provided scoped lock embedded.
	template<typename L>
	rebind<T, LinkP, std::remove_reference_t<L>> lock( L &&lk ) const{
		return {m_linkp, std::move(lk)};
		//return lock_rebound_impl<std::remove_reference_t<L>, linked_list_iterator>(std::move(lk));
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

	bool operator==(const linked_list_iterator &rhs) const{
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
	using cv_pointer_type = const volatile T *;

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
		if(obj){
			auto thisp2 = static_cast<this_type *>(thisp);
			return to_byte_ptr(obj) - to_byte_ptr( traits_type::base_ptr( *thisp2 ) );
		}
		else
			return 0;
	}

	//template<typename This, typename = jab::meta::permit_any_cv<This, RelPtr<T>> >
	//static pointer_type make_pointer( This *th, ::size_t offs){
	//	return jab::util:static_const_cast<pointer_type>( static_cast<void *>(to_byte_ptr(th) + offs)  );
	//}

	/*
	template< typename This, typename = jab::meta::permit_any_cv<This, RelPtr> >
	inline static pointer_type make_pointer(This *thisp){
		using voidp_type = typename jab::util::copy_cv<This, void>::type *;
		using this_type = typename jab::util::copy_cv<This, typename traits_type::RelPtr_type>::type;

		auto thisp2 = static_cast<this_type *>(thisp);
		return jab::util::reinterpret_const_cast<pointer_type>( reinterpret_cast<voidp_type>(to_byte_ptr( traits_type::base_ptr(*thisp2) ) + thisp->offset()) );
	}
	*/

	inline pointer_type make_pointer() const{
		auto thisc = reinterpret_cast<const volatile char *>(this);
		auto p = thisc + offset();
		auto p2 = reinterpret_cast<cv_pointer_type>(p);
		return const_cast<pointer_type>(p2);
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
		m_offset( make_offset(ptr, this) ){}

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

	//Same as copying
	inline RelPtr &operator=( RelPtr &&rhs){
		return operator=(rhs);
	}

	inline RelPtr &operator=( pointer_type ptr ){
		m_offset = make_offset(ptr, this);
		return *this;
	}
	
	inline pointer_type operator->(){
		return make_pointer();
	}

	inline pointer_type operator->() const{
			return make_pointer();
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
//This assumes that it's not self-relative like the basic RelPtr<> is, so
//it just copies itself without any relocation or pointer math
//
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
	/*
	OffsetPtr(pointer_type pobj, base_ptr_function func):
	base_type(pobj),		
		m_base_f(func){}	
	*/
	OffsetPtr(pointer_type pobj, base_ptr_function func):
		base_type(),
		m_base_f(func){

		//Has to be done here otherwise the base will initialize before the functor is set
		this->base_type::operator=(pobj);
	}

	OffsetPtr(const OffsetPtr &rhs):
		base_type(rhs),
		m_base_f(rhs.m_base_f){			
	}		


	auto base_ptr() const{
		return m_base_f();
	}

	OffsetPtr &operator=(const OffsetPtr &rhs){
		m_base_f = rhs.m_base_f;
		this->base_type::operator=( rhs );
		return *this;
	}

	//Same as copying
	OffsetPtr &operator=(OffsetPtr &&rhs){
		return operator=(rhs);
	}

	OffsetPtr &operator=(pointer_type ptr){
		base_type::operator=(ptr);
		return *this;
	}
	
	/*
	OffsetPtr &operator=(OffsetPtr &&rhs){
		this->base_type::operator=( std::move(rhs) );
		m_base_f = std::move( rhs.m_base_f );
	}
	*/
};

template<typename T>
struct linked_list_link{
	using value_type = T;
	using link_type = linked_list_link<T>;
	RelPtr<value_type> value_ptr;
	RelPtr<linked_list_link> next;
};

//It's just a linked_list_link, but the type formalizes that it's the head of the list
template<typename T>
struct linked_list:public linked_list_link<T>{};

template<typename T>
struct doubly_linked_list_link{
	using value_type = T;
	RelPtr<value_type> value_ptr;
	RelPtr<doubly_linked_list_link> next, prev;
};

template<typename T>
struct doubly_linked_list:public doubly_linked_list_link<T>{};

/*
template<typename T>
struct doubly_linked_list:public linked_list<T>{
private:
	using base_type = linked_list<T>;

public:
	RelPtr<linked_list<T>> next;

	template<typename LockType>
	auto lock_rebound(LockType &&lock){
		//eturn base_type::lock_rebound_impl<>
	}

};
*/
}

//Provided a pointer to the head node of a linked list,
//provide a view on it that resembles an ordinary container
template<typename LinkP, typename Alloc>
requires jab::util::is_dereferenceable<LinkP>
struct linked_list_view{

public:
	using link_pointer = LinkP;	
	using link_type = typename std::pointer_traits<link_pointer>::element_type;
	using value_type = typename std::pointer_traits<decltype(link_type::value_ptr)>::element_type;
	using value_pointer = typename std::pointer_traits<link_pointer>::template rebind<value_type>;
	using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<value_type>;
	using link_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<link_type>;	

protected:
	using allocator_traits = std::allocator_traits<allocator_type>;
	using link_allocator_traits = std::allocator_traits<link_allocator_type>;

	allocator_type m_alloc;
	link_allocator_type m_link_alloc;
	link_pointer m_head;

public:
	using iterator_type = linked_list_iterator<value_type, link_pointer>;
	using const_iterator_type = linked_list_iterator<const value_type, link_pointer>;

private:
	static const_iterator_type discern_iterator_f( const linked_list_view * );
	static iterator_type discern_iterator_f( linked_list_view * );
	
	template<typename U>
	using discern_iterator_t = decltype( discern_iterator_f( std::declval<U>() ) );

	template<class This>
	static discern_iterator_t<This *> begin_impl( This *thisp ){
		auto tmp = thisp->m_head;
		tmp = &*thisp->m_head->next;
		return { tmp };
	}

	template<class This>
	static discern_iterator_t<This *> end_impl( This *thisp ){
		return { nullptr };
	}

	void link_front( value_type *vp ){
		auto lp = new( link_allocator_traits::allocate(m_link_alloc, 1) ) link_type();
		lp->value_ptr = vp;
		lp->next = m_head->next;
		m_head->next = lp;
	}

public:
	linked_list_view():
		m_alloc(),
		m_link_alloc(),
		m_head(nullptr){}

	linked_list_view( link_pointer link, const allocator_type &alloc ):
		m_alloc(alloc),
		m_link_alloc(m_alloc),
		m_head(link){}

	//This is kind of dangerous because you could accidentally link an object outside the
	//file address space
	/*
	void push_front(value_type &v){		
		link_front(v);
	}
	*/

	value_type &push_front(value_type &&v){
		auto objp = new( allocator_traits::allocate(m_alloc, 1) ) value_type( std::move(v) );
		link_front(objp);
		return *objp;
	}
	
	auto begin() const{
		return begin_impl(this);
	}

	auto begin(){
		return begin_impl(this);
	}
	
	auto end() const{
		return end_impl(this);
	}

	auto end(){
		return end_impl(this);
	}

	auto cbegin() const{
		return begin_impl(this);
	}

	auto cend() const{
		return end_impl(this);
	}
};

}
