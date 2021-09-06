#pragma once
#include <type_traits>
#include <functional>

namespace jab{
namespace util{
        
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

}
}
