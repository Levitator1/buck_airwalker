#pragma once
#include <atomic>
#include <iostream>
#include <string>
#include <sstream>
#include <mutex>
#include <functional>
#include "concurrency/thread_pool.hpp"
#include "concurrency/stream_forwarder.hpp"
#include "compiler.hpp"

/*
* Just a central place to put console messaging in, mainly for the purposes of exclusive locking,
* so that threads don't talk over each other.
*/
namespace jab::util{

class Console;

namespace impl{
inline static auto make_console_output_task(std::ostream *stream, const std::string &msg){
	return [stream, msg](){ 
		if(stream){
			*stream << msg << std::flush;
			return 0;
		}
		else{
			return -1;
		}
	};
}
}

struct ConsoleErrorHandler{
	void operator()( const std::exception_ptr &ptr) const;
};

struct ConsoleTypes{
	using mutex_type = std::recursive_mutex;
	using lock_type = std::unique_lock<mutex_type>;
	//using ref_type = std::reference_wrapper<Console>;
	using task_type = decltype( impl::make_console_output_task(nullptr, {}) );
	using thread_pool_type = levitator::concurrency::ThreadPool<task_type, ConsoleErrorHandler>;	
};

class ConsoleStreamBase : public ConsoleTypes{
protected:
	Console *p_console;

public:
	ConsoleStreamBase(Console &cons);
};

template<class This>
class ConsoleOutputBase : 
	public ConsoleStreamBase, 
	public levitator::concurrency::ostream_forwarder<This, std::ostream>{

	using base_type = ConsoleStreamBase;

protected:
	std::stringstream p_sstream;

public:	
	using base_type::base_type;

	std::stringstream &get_ostream(){
		return p_sstream;
	}
};

class ConsoleInput : 
	public ConsoleStreamBase, 	
	public levitator::concurrency::istream_forwarder<ConsoleInput, std::istream>{
	using base_type = ConsoleStreamBase;
	using lock_type = ConsoleTypes::lock_type;

	lock_type m_lock;

public:	
	ConsoleInput( Console &cons,  lock_type &&lock);
	std::istream &get_istream() const;
};

//TODO:
//Oddly, when you chain more than one operation on a line with the stream, the compiler
//wants to perform an object copy. This is kind of inefficient, but not enough
//to matter for the current project.
class ConsoleOutBuffer : public ConsoleOutputBase<ConsoleOutBuffer>{
	using base_type = ConsoleOutputBase<ConsoleOutBuffer>;

public:	
	ConsoleOutBuffer( Console & );
	ConsoleOutBuffer(ConsoleOutBuffer &&);
	ConsoleOutBuffer(const ConsoleOutBuffer &);
	~ConsoleOutBuffer();
};

class ConsoleErrBuffer : public ConsoleOutputBase<ConsoleErrBuffer>{
	using base_type = ConsoleOutputBase<ConsoleErrBuffer>;

public:
	ConsoleErrBuffer( Console & );
	ConsoleErrBuffer(ConsoleErrBuffer &&);
	ConsoleErrBuffer(const ConsoleErrBuffer &);
	~ConsoleErrBuffer();
};

//This warning seems to be because a template is defined in terms of a lambda's type, which
//I guess is in the anonymous namespace, and that results in a warning
PUSH_WARN
WARN_DISABLE_SUBOBJECT_LINKAGE
class Console : public ConsoleTypes{

	using message_type = std::function< void () >;
	using mutex_type = ConsoleTypes::mutex_type;
	mutex_type m_in_mutex;
	
	using task_type = ConsoleTypes::task_type;
	using thread_pool_type = ConsoleTypes::thread_pool_type;
	thread_pool_type m_queue = { 1, ConsoleErrorHandler(), impl::make_console_output_task(nullptr, {}) };	

public:
	using in_type = ConsoleInput;
	using out_type = ConsoleOutBuffer;
	using err_type = ConsoleErrBuffer;

	std::atomic<std::istream *> in_stream_pointer = nullptr;
	std::atomic<std::ostream *> out_stream_pointer = nullptr, error_stream_pointer = nullptr;

	//Requires an explicit call to init in order to avoid static fiasco in pre-main
	void init();

	in_type in();
	out_type out();
	err_type err();

	void queue_out(const std::string &msg);
	void queue_err(const std::string &msg);
};
POP_WARN

extern Console console;

//Says something...
//and then commits to saying "FAILED" unless notified to say "OK".
//This ends up being kind of problematic because it needs exclusive access
//to the output stream until the outcome is decided.
class EllipsisGuard{
    std::string m_current_outcome, m_success_string;

public:
    EllipsisGuard(const std::string &msg, const std::string &ok_string="OK", const std::string &fail_string="FAIL");
    ~EllipsisGuard();
	void outcome(const std::string &str);
    void ok();
};

}
