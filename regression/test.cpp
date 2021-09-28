#include <cstdlib>
#include <string>
#include <iostream>
#include "test.hpp"

using namespace std;

EllipsisGuard::EllipsisGuard( const string &msg ){
    cout << msg;
}

EllipsisGuard::~EllipsisGuard(){
    cout << (m_ok ? "OK" : "FAILED") << endl;
}

void EllipsisGuard::ok(){
    m_ok = true;
}

RandStream::RandStream(value_type seed):
	m_seed(seed){}

//Reseed the RNG with its previous output so that multiple streams
//can be used and resumed at any time
RandStream::value_type RandStream::get(){
	srand(m_seed);
	return m_seed = rand();
}

RandStream::value_type RandStream::seed() const{
	return m_seed;
}

//Return an int within [minint, max_plus1)
int RandStream::int_between(int minint, int max_plus1){
	const auto width = max_plus1 - minint;
	return get() % width + minint;
}

