#include "console.hpp"

using namespace jab::util;
using namespace levitator::concurrency;

void Console::init(){
	m_inp = &std::cin;
	m_outp = &std::cout;
	m_errp = &std::cerr;
}

Console::in_type Console::in(){
	return { *m_inp, m_in_mutex };
}

Console::out_type Console::out(){
	return { *m_outp, m_in_mutex };
}
Console::err_type Console::err(){
	return { *m_errp, m_in_mutex };
}
