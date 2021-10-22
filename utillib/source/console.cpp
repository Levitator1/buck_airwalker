#include "concurrency/concurrency.hpp"
#include "console.hpp"

using namespace jab::util;
using namespace levitator::concurrency;

//Global singleton
Console jab::util::console;

void Console::init(){
	in_stream_pointer = &std::cin;
	out_stream_pointer = &std::cout;
	error_stream_pointer = &std::cerr;
}

Console::in_type Console::in(){
	return {  *this, lock_type(m_in_mutex) };
}

Console::out_type Console::out(){
	return {  *this };
}
Console::err_type Console::err(){
	return { *this };
}

ConsoleOutBuffer::~ConsoleOutBuffer(){
	p_console->queue_out( p_sstream.str() );
}

ConsoleErrBuffer::~ConsoleErrBuffer(){
	p_console->queue_err( p_sstream.str() );
}

void Console::queue_out( const std::string &msg ){
	m_queue.push( jab::util::impl::make_console_output_task(out_stream_pointer, msg ) );
}

void Console::queue_err( const std::string &msg ){	
	m_queue.push( jab::util::impl::make_console_output_task(error_stream_pointer, msg ) );
}

ConsoleStreamBase::ConsoleStreamBase(Console &cons):p_console(&cons){
}

ConsoleInput::ConsoleInput( Console &cons, lock_type &&lock ):
	base_type(cons),
	m_lock( std::move(lock)){}

std::istream &ConsoleInput::get_istream() const{
	return *p_console->in_stream_pointer;
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