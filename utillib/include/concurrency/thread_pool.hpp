#pragma once
#include <memory>
#include <thread>
#include <vector>
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

class ThreadPool;

class PoolThread:public std::thread::thread {
	static void run(ThreadPool &);

public:
	PoolThread(ThreadPool &);	
};

class ThreadPool{
public:
	using task_pointer = std::unique_ptr<Task>;

private:
	MessageQueue<task_pointer> m_queue;
	std::vector<PoolThread> m_threads;

public:
	ThreadPool(int count = std::thread::hardware_concurrency() );
	~ThreadPool();
	void shutdown();

	void push( task_pointer && );
	task_pointer pop();
	void stop();
};

}
