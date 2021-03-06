#pragma once
#include <stdexcept>
#include <optional>
#include <type_traits>
#include <functional>
#include <utility>
#include <string>
#include "meta.hpp"

namespace jab{
namespace util{

namespace impl{

template<typename T, bool has_move = std::is_move_assignable_v<T>>
struct move_or_copy_impl{
	using value_type = const std::remove_reference_t<T>;
	using input_ref_type = value_type &;
	using result_ref_type = value_type &;
	static constexpr bool moves = false;
	
	static result_ref_type pass(input_ref_type v){
		return v;
	}
};

template<typename T>
struct move_or_copy_impl<T, true>{
	using value_type = std::remove_reference_t<T>;
	using input_ref_type = value_type &;
	using result_ref_type = value_type &&;
	static constexpr bool moves = true;
	
	static result_ref_type pass(input_ref_type v){
		return std::move(v);
	}
};

}

//Move something if it is movable, preferring move, or fall back to copying otherwise
//Well, this doesn't actually take any action. It works like a conditional version of std::move(),
//which instead sometimes returns a move-reference and other times a const reference.
template<typename T>
class move_or_copy : public impl::move_or_copy_impl<T>{

	using base_type = impl::move_or_copy_impl<T>;	
	using value_type = typename base_type::value_type;
	value_type *m_obj;
	
public:
	using input_ref_type = typename base_type::input_ref_type;
	using result_ref_type = typename base_type::result_ref_type;

	move_or_copy():
		m_obj(nullptr){}

	move_or_copy( input_ref_type v ):
		m_obj(&v){}

	result_ref_type pass() const{
		return base_type::pass(*m_obj);
	}

	result_ref_type operator()() const{
		return pass();
	}

	operator result_ref_type() const{
		return pass();
	}

};

template<typename T>
move_or_copy( T &&v ) -> move_or_copy< std::remove_reference_t<T> >;

//emptiest type possible
struct null_type{};

//Ensure that a narrow string is less than a specified length
::size_t strnlenlt(const char *str, ::size_t n);

template<::size_t N>
::size_t strnlenlt( const char str[N] ){
	return strnlenlt( str, N );
}

//The intersection of two numeric ranges [A, B], [C, D]
template<typename T>
std::optional<std::pair<T, T>> range_intersect( T a, T b, T c, T d ){
	if(a > d || c > b)
		return {};
	else
		return { {std::max(a, c), std::min(b, d)} };
}

//Allows a single class to offer multiple begin()/end() ranges
template<class BeginF, class EndF>
class range_property{
	const BeginF m_beginf;
	const EndF m_endf;

public:
	range_property(BeginF beg, EndF endf):
		m_beginf(beg),
		m_endf(endf){}

	auto begin(){ return m_beginf(); }
	auto begin() const { return m_beginf(); }
	auto end(){ return m_endf(); }
	auto end() const { return m_endf(); }
};

template<class BeginF, class EndF>
range_property(BeginF beg, EndF en) -> range_property<BeginF, EndF>;

//Do one of two things on scope exit
template<typename Ft, typename Ff>
class cond_guard{
    Ft f_true;
    Ff f_false;

public:
    bool status = false;
    
    cond_guard(Ft ft, Ff ff):
        f_true(ft), 
        f_false(ff){}
        
    ~cond_guard(){
        if(status)
            f_true();
        else
            f_false();
    }
};

template<typename Ft, typename Ff>
cond_guard(Ft ft, Ff ff) -> cond_guard<Ft, Ff>;

template<typename T>
class as_bin;

//A wrapper that means to interpret something as binary data
template<typename T>
class as_bin<T &>:public std::reference_wrapper<T>{
public:
    using std::reference_wrapper<T>::reference_wrapper;
};

// Make a copy if not by reference
template<typename T>
class as_bin:public std::reference_wrapper<T>{
    T m_obj;
    
public:
    as_bin(T &&obj):
        std::reference_wrapper<T>(m_obj),
        m_obj(std::move(obj)){}

};

template<typename T>
as_bin(T &) -> as_bin<T &>;

template<typename T>
as_bin(T &&) -> as_bin<T>;

//If T then U
//If const T then const U
template<typename T, typename U>
struct copy_const{
    using type = U;
};

template<typename T, typename U>
struct copy_const<const T, U>{
    using type = const U;
};

template<typename T, typename U>
struct copy_volatile{
    using type = U;
};

template<typename T, typename U>
struct copy_volatile<volatile T, U>{
    using type = volatile U;
};

template<typename T, typename U>
struct copy_cv{
	using type = typename copy_volatile<T, typename copy_const<T, U>::type>::type;
};

template<typename T, typename U>
using copy_cv_t = typename copy_cv<T,U>::type;

//Combines the actions of both static_cast and const_cast
//On some systems read-only objects are in their own address space, but that doesn't necessarily
//map directly to constness because you can const a pointer or reference to a non-const thing without moving it.
//So we just cast away without any regard to CV qualifiers.
template<typename T, typename U>
T static_const_cast(U v){

	//Silently convert to most qualified form of T.
	///Then const-cast away unnneeded qualifiers
	using V = std::add_cv_t<T>;

	return const_cast<T>(static_cast<V>(v));	
}

//Reinterpret and const cast together
template<typename T, typename U>
T reinterpret_const_cast(U v){
	
	using V = jab::util::copy_cv_t<decltype(v), T>;
	return const_cast<T>(reinterpret_cast<V>(v));	
}

//T * is what is being cast to (const) char *
template<typename T>
struct binary_traits_base{
    static_assert( std::is_trivially_copyable<T>::value );
    using pointer_type = typename copy_const<T, char>::type *;
    using element_type = T;
};

//Traits types for interpreting something as binary
template<typename T>
struct simple_binary_traits:public binary_traits_base<T>{
    
    using Base = binary_traits_base<T>;
    using value_type = T;
    
    static typename Base::pointer_type data(T &obj){
        return static_cast<typename Base::pointer_type>(&obj);
    }
    
    static ::size_t length(const T &obj){ return sizeof(T); }
};

//Special case for arrays
template<typename T, ::size_t S>
struct simple_binary_traits<T[S]>:public binary_traits_base<T>{

    using Base = binary_traits_base<T>;
    using value_type = T[S];
    
    static typename Base::pointer_type data(value_type array){
        //Enforces trivial-copy
        return simple_binary_traits<T>::data(array[0]);
    }
    
    static size_t length(value_type array){
        if(S)
            return sizeof(T) * S;
        else
            return 0;
    }
};

template<typename T>
struct binary_traits:public simple_binary_traits<T>{};

//Specialize for strings
template<typename CharT, typename Traits, typename Alloc>
struct binary_traits< std::basic_string<CharT, Traits, Alloc> >:public binary_traits_base<const CharT>{
    using Base = binary_traits_base<const CharT>;
    using value_type = std::basic_string<CharT, Traits, Alloc>;
    
    static typename Base::pointer_type data(const value_type &str){
        if(str.length())
            return static_cast<typename Base::pointer_type>(str.data());
        else
            return nullptr;
    }
    
    static size_t length(const value_type &str){ return str.length() * sizeof(CharT); }
};

//Specialize for vectors
template<typename T>
struct binary_traits<std::vector<T>>:public binary_traits_base<T>{
    using Base = binary_traits_base<T>;
    using value_type = std::vector<T>;
    
    static typename Base::pointer_type data(value_type &vect){
        
        //Go through traits to enforce trivial-copy
        if(vect.size())
            return binary_traits<T>::data(vect[0]);
        else
            return nullptr;
    }
    
    static ::size_t length(const value_type &vect){
        return vect.size() * sizeof(T);
    }
};

//Replace all of the non-printing characters in a string with a hex representation
std::string hex_format(const std::string &);

//If T is non-const, then return T
//If T is const, then make a copy and return that (for use as a temporary)
template<typename T>
class deconst{
    T &m_ref;
    
public:
    deconst(T &ref):m_ref(ref){}
    T &get() const{ return m_ref; }
};

template<typename T>
class deconst<const T>{
    T m_copy;
    
public:
    deconst(const T &v):m_copy(v){}
    T &get(){ return m_copy; } 
    const T &get() const{ return m_copy; }
};

template<typename T>
class deconst<T &&>{
    T m_copy;
    
public:
    deconst(T &&v):m_copy( std::forward<T>(v) ){}
    T &get(){ return m_copy; }
    const T &get() const{ return m_copy; }
};

template<typename T>
deconst(T &) -> deconst<T>;

template<typename T>
deconst(T &&) -> deconst<T &&>;

//Implement operator overloads to delegate operations to T
//"This" is the derived class. This must be constructable from T for the binary operators to work: This(const T &)
template<typename T, typename This>
struct delegate_arithmetic{
	T *objp;

	This *upcast(){ return static_cast<This *>(this); }
	const This *upcast() const { return static_cast<const This *>(this); }

private:
	T &value(){ return *objp; }
	const T &value() const{ return *objp; }

protected:

	template<typename U>
	using This_or_T = meta::permit_types<U, This, T>;

	delegate_arithmetic(T &v):
		objp(&v){}

public:
	operator bool() const{
		return value();
	}

	bool operator!() const{
		return !operator bool();
	}

	bool operator==( const T &rhs ) const{
		return value() == rhs;
	}

	This &operator++(){ 
		++value();
		return *upcast();
	}

	T operator++(int){ 
		auto prev = *objp;
		++value();
		return prev;
	}

	This &operator--(){ 
		--value();
		return *upcast();
	}

	T operator--(int){ 
		auto prev = *objp;
		--value();
		return prev;
	}

	This &operator|=(const T &rhs){
		value() |= rhs;
		return *upcast();
	}

	This &operator&=(const T &rhs){
		value() &= rhs;
		return *upcast();
	}

	This operator~() const{
		return { ~value() };
	}

	This &operator*=(const T &rhs){
		value()*=rhs;
		return *upcast();
	}

	This &operator/=(const T &rhs){
		value()/=rhs;
		return *upcast();
	}

	This operator+(const T &rhs) const{
		return { value() + rhs };
	}

	This operator-(const T &rhs) const{
		return { value() - rhs };
	}

	This operator*(const T &rhs) const{
		return { value() * rhs };
	}

	This operator/(const T &rhs) const{
		return { value() / rhs };
	}

	This operator|(const T &rhs) const{
		return { value() | rhs };
	}

	This operator&(const T &rhs) const{
		return { value() & rhs };
	}

	This operator^(const T &rhs) const{
		return { value() ^ rhs };
	}

	This operator>>(int rhs) const{
		return { value() >> rhs };
	}

	This operator<<(int rhs) const{
		return { value() << rhs };
	}

};

namespace impl{

	//duplicated from util.hpp
	template<typename... Args>
	void null_func(Args... args){		
	}

}

//Same as align-of, but will return 1 for the alignment of void without producing a warning
template<typename T>
class alignof_any:public std::integral_constant<int, alignof(T)>{	
};

template<>
class alignof_any<void>:public std::integral_constant<int, 1>{
};

template<typename T>
concept is_dereferenceable = requires(T p) {
    impl::null_func(p.operator->());
	impl::null_func(p.operator*());
};

//General case of a scoped guard object that does a thing when it goes away
template<typename F>
class Guard{
public:
	using function_type = F;

private:
	function_type m_func;

public:
	Guard( const F &f = {} ):m_func(f){}
	Guard( F &&f ):m_func(std::move(f)){}
	~Guard(){ m_func(); }
};

template<typename F>
Guard( F &&f ) -> Guard<F>;

}
}
