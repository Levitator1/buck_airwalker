#pragma once

#include <functional>
#include <mutex>
#include <iostream>
#include "concurrency.hpp"

/*
* This inelegant mess is so that you can have a class which forwards the functionality of a stream, 
* but the stream operators return the forwarding class and not the internal stream object. The 
* IO operators are kind of a crap arrangement from before the language was mature enough to do much
* better.
*/

namespace levitator{
namespace concurrency{

template<typename This, class Stream>
class ostream_forwarder;

template<typename This, typename CharT, typename Traits>
class ostream_forwarder< This, std::basic_ostream<CharT, Traits>> {
    
public:
	using this_type = This;	
	using ostream_type = std::basic_ostream<CharT, Traits>;

private:
	this_type *downcast(){
		return static_cast<this_type *>(this);
	}

	const this_type *downcast() const{
		return static_cast<const this_type *>(this);
	}

public:

	operator bool() const{
		return downcast()->get_ostream().operator bool();
	}

	bool operator!() const{
		return operator bool();
	}

    //general insertion
    template<typename T2>
    this_type &operator<<( T2 &&x){
        downcast()->get_ostream() << std::forward<T2>(x);
        return *downcast();
    }

    //manipulator insertion
    //needed for overload resolution
    this_type &operator<<( ostream_type &(manip)(ostream_type &) ){
        downcast()->get_ostream() << manip;
        return *downcast();
    }
};

template<typename This, class Stream>
class istream_forwarder;

template<typename This, typename CharT, typename Traits>
class istream_forwarder< This, std::basic_istream<CharT, Traits>> {
    
public:
	using this_type = This;	
	using istream_type = std::basic_istream<CharT, Traits>;

private:
	this_type *downcast(){
		return static_cast<this_type *>(this);
	}

	const this_type *downcast() const{
		return static_cast<const this_type *>(this);
	}

public:
	operator bool() const{
		return downcast()->get_istream().operator bool();
	}

	bool operator!() const{
		return operator bool();
	}

    //general insertion
    template<typename T2>
    this_type &operator>>( T2 &&x){
        downcast()->get_istream() >> std::forward<T2>(x);
        return *downcast();
    }
};

}
}
