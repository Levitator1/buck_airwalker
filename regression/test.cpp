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

