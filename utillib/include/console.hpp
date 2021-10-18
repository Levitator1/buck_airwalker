#pragma once
#include <atomic>
#include <iostream>
#include <string>
#include <sstream>
#include <mutex>
#include <functional>
#include "concurrency/thread_pool.hpp"
#include "concurrency/locked_ostream_proxy.hpp"
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

struct ConsoleTypes{
	using mutex_type = std::recursive_mutex;
	using ref_type = std::reference_wrapper<Console>;
	using task_type = decltype( impl::make_console_output_task(nullptr, {}) );
	using thread_pool_type = levitator::concurrency::ThreadPool<task_type>;
};


class ConsoleBufferBase : public ConsoleTypes, public std::stringstream, public ConsoleTypes::ref_type{
public:
	using ref_type = ConsoleTypes::ref_type;
	using ref_type::ref_type;
};

class ConsoleOutBuffer : public ConsoleBufferBase{
public:
	using ConsoleBufferBase::ConsoleBufferBase;
	~ConsoleOutBuffer();
};

class ConsoleErrBuffer : public ConsoleBufferBase{
public:
	using ConsoleBufferBase::ConsoleBufferBase;
	~ConsoleErrBuffer();
};

//This warning seems to be because a template is defined in terms of a lambda's type, which
//I guess is in the anonymous namespace, and that results in a warning
PUSH_WARN
WARN_DISABLE_SUBOBJECT_LINKAGE

class Console : public ConsoleTypes{

	using message_type = std::function< void () >;
	using mutex_type = ConsoleTypes::mutex_type;
	mutex_type m_in_mutex, m_out_mutex, m_err_mutex;
	std::atomic<std::istream *> m_inp = nullptr;
	std::atomic<std::ostream *> m_outp = nullptr, m_errp = nullptr;

	using task_type = ConsoleTypes::task_type;
	using thread_pool_type = ConsoleTypes::thread_pool_type;
	thread_pool_type m_queue = {1, impl::make_console_output_task(nullptr, {})};	

public:
	using in_type = levitator::concurrency::locked_istream_ref<std::istream, mutex_type>;
	using out_type = ConsoleOutBuffer;
	using err_type = ConsoleErrBuffer;

	//Requires an explicit call to init in order to avoid static fiasco in pre-main
	void init();

	in_type in();
	out_type out();
	err_type err();

	void queue_out(const std::string &msg);
	void queue_err(const std::string &msg);
};
POP_WARN

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

extern Console console;

}
