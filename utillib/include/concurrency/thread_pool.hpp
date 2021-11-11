#pragma once
#include <memory>
#include <thread>
#include <vector>
#include "exception.hpp"
#include "util.hpp"
#include "MessageQueue.hpp"

namespace levitator::concurrency {

class Task{
protected:
	virtual void proc() = 0;

public:
	virtual ~Task() = default;
	virtual int operator()();
};

class TerminateTask:public Task{
protected:
	virtual void proc() override;

public:
	virtual int operator()() override;
};


/*
class ThreadPool;
class PoolThread:public std::thread::thread {
	static void run(ThreadPool &);

public:
	PoolThread(ThreadPool &);	
};
*/

template<class, class, typename>
class ThreadPool;

template<class Task, class ExHandler>
class PoolThread:public std::thread::thread{

public:
	using task_type = Task;	
	using thread_pool_type = ThreadPool<task_type, ExHandler, PoolThread>;

private:

	static int pool_thread_proc( thread_pool_type &pool){		
		int result;
		do{
			auto task = pool.pop();

			try{
				result = task();
			}
			catch(...){
				pool.exception_handler()( std::current_exception() );
			}

		}while( result  == 0 );

		return result;
	}

public:
	//Caution because m_ex_handler is not initialized until after the thread has started.
	//But then, there shouldn't be any messages in the queue until after initialization is done either.
	//And hopefully we're not going to throw just waiting on the queue mutex. Hopefully.
	PoolThread(thread_pool_type &pool):
		std::thread::thread::thread(pool_thread_proc, std::ref(pool)){}
};

/*
//A pool thread type that knows to dereference queue entries as pointers
template<class TaskP>
class PointerPoolThread:public PoolThreadBase<ThreadPool<TaskP, PointerPoolThread<TaskP>>, int (*)(const TaskP &task)>{
	using base_type = PoolThreadBase<ThreadPool<TaskP, PointerPoolThread<TaskP>>, int (*)(const TaskP &task)>;

public:
	using thread_pool_type = typename base_type::thread_pool_type;
	using task_type = typename base_type::task_type;

private:	
	static int handler_proc( const task_type &taskp ){
		if( !taskp )
			return -1;
		else
			return (*taskp)();
	}

public:		
	PointerPoolThread(thread_pool_type &pool):
		base_type( pool, &handler_proc){}

};
*/


class DefaultThreadPoolExceptionHandler:public jab::exception::DefaultBackgroundExceptionHandler{
	using base_type = jab::exception::DefaultBackgroundExceptionHandler;
public:
	static constexpr char message[] = "Unexpected exception in thread pool thread...";
	DefaultThreadPoolExceptionHandler();
};

//Task must be constructible from ThreadPool reference
//Should launch a thread process on construction, as std::thread does
template<class Task, 
	class ExHandler = jab::exception::DefaultBackgroundExceptionHandler,
	typename Thread = PoolThread<Task, ExHandler>>
class ThreadPool{
public:
	using task_type = Task;
	using exception_handler_type = ExHandler;
	using thread_type = Thread;

private:
	exception_handler_type m_exception_handler;
	task_type m_terminate_task;	
	MessageQueue<task_type> m_queue;
	std::vector<thread_type> m_threads;

	void join_each(){
		for( auto &t : m_threads ){
			if(t.joinable()) 
				t.join();
		}
	}

public:
	ThreadPool(
		int count = std::thread::hardware_concurrency(),
		const exception_handler_type &exh = {},
		typename jab::util::move_or_copy<task_type>::result_ref_type terminate = {} ):
			m_exception_handler(exh),
			m_terminate_task( jab::util::move_or_copy(terminate) ){

		m_threads.reserve(count);
		for( int i = 0; i < count; ++i ){
			m_threads.push_back( { *this} );
		}
	}

	~ThreadPool(){	
		shutdown_now();
	}

	exception_handler_type &exception_handler() const{
		return m_exception_handler;
	}

	exception_handler_type &exception_handler(){
		return m_exception_handler;
	}

	//Shut down each thread as soon as it pulls a task
	void shutdown_now(){	
		for(auto &t : m_threads){
			if(t.joinable())
				m_queue.push_front(  jab::util::move_or_copy(m_terminate_task)  );
		}

		join_each();
	}

	//Shut down each thread after all pending tasks are done
	void shutdown(){	
		for(auto &t : m_threads){
			if(t.joinable())
				m_queue.push_back(  jab::util::move_or_copy(m_terminate_task)  );
		}

		join_each();
	}

	void push( task_type&& task ){
		m_queue.push_back( std::move(task) );
	}

	void push( const task_type& task ){
		m_queue.push_back( task );
	}

	task_type pop(){
		return m_queue.pop();
	}
};

}
