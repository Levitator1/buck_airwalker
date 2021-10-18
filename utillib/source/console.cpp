#include "concurrency/concurrency.hpp"
#include "console.hpp"

using namespace jab::util;
using namespace levitator::concurrency;

//Global singleton
Console jab::util::console;

void Console::init(){
	m_inp = &std::cin;
	m_outp = &std::cout;
	m_errp = &std::cerr;
}

Console::in_type Console::in(){
	return { *m_inp, m_in_mutex };
}

Console::out_type Console::out(){
	return { *this };
}
Console::err_type Console::err(){
	return { *this };
}

ConsoleOutBuffer::~ConsoleOutBuffer(){
	ref_type::get().queue_out( str() );
}

ConsoleErrBuffer::~ConsoleErrBuffer(){
	ref_type::get().queue_err( str() );
}

void Console::queue_out( const std::string &msg ){
	m_queue.push( jab::util::impl::make_console_output_task(m_outp, msg ) );
}

void Console::queue_err( const std::string &msg ){	
	m_queue.push( jab::util::impl::make_console_output_task(m_errp, msg ) );
}

EllipsisGuard::EllipsisGuard(const std::string &msg, const std::string &ok_string, const std::string &fail_string):
	m_current_outcome(fail_string),
	m_success_string(ok_string){

		std::cout  << msg << std::flush;
}

EllipsisGuard::~EllipsisGuard(){
    std::cout << m_current_outcome << std::endl;
}

void EllipsisGuard::outcome( const std::string &msg ){
	m_current_outcome = msg;
}

void EllipsisGuard::ok(){
	outcome( m_success_string );
}