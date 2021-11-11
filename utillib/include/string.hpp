#pragma once
#include <algorithm>

namespace jab::util{

template<typename Ch, class Traits = std::char_traits<Ch>>
auto toupper( const std::basic_string<Ch, Traits> &in ){
	std::basic_string<Ch, Traits> result;
	result.reserve(in.size());

	for( auto &c : in){
		result.push_back( ::toupper(c) );
	}
	return result;
}

}
