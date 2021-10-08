#include <memory>
#include <vector>
#include "concurrency/thread_pool.hpp"

using namespace levitator::concurrency;

int Task::operator()(){
	proc();
	return 0;
}

//Task object that notifies a worker thread to exit
void TerminateTask::proc(){}

int TerminateTask::operator()(){
	return -1;
}

static void thread_proc(ThreadPool &pool){
	decltype(pool.pop()) ptask;
	do{
		ptask = pool.pop();
	}while( (*ptask)() == 0 );
}

PoolThread::PoolThread(ThreadPool &pool):
	thread(thread_proc, std::ref(pool)){
}

ThreadPool::ThreadPool( int n ){

	m_threads.reserve(n);
	for( int i; i < n; ++i ){
		m_threads.push_back( { *this } );
	}
}

void ThreadPool::shutdown(){
	
	for(auto &t : m_threads){
		if(t.joinable())
			m_queue.push_front(  task_pointer(new TerminateTask())  );
	}

	for( auto &t : m_threads ){
		if(t.joinable()) 
			t.join();
	}
}

ThreadPool::~ThreadPool(){	
	shutdown();
}

void ThreadPool::push( task_pointer&& task ){
	m_queue.push_back( std::move(task) );
}

ThreadPool::task_pointer ThreadPool::pop(){
	return m_queue.pop();
}