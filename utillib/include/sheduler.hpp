#pragma once
#include <mutex>
#include <condition_variable>
#include <thread>
#include <map>
#include <chrono>
#include "exception.hpp"

//Manage a list of things to do in the future because
//std::async does not seem to have good support for canceling tasks
template<class Task, class ExHandler = jab::exception::DefaultBackgroundExceptionHandler>
class Scheduler{
public:
	using task_type = Task;
	using clock_type = std::chrono::high_resolution_clock;
	using time_type = clock_type::time_point;

private:
	using map_type = std::multimap<time_type, task_type>;	

	using mutex_type = std::mutex;
	using lock_type = std::unique_lock<mutex_type>;

	mutex_type m_mutex;
	std::condition_variable m_cv;
	map_type m_schedule;
	std::thread m_thread;
	bool m_terminate = false;

	void thread_proc(){

		auto lock = lock_type(m_mutex);
		
		while( !m_terminate ){
			if( m_schedule.size() > 0 ){
				auto next = m_schedule.begin();
				auto next_time = next->first;
				auto &next_task = next->second;

				//If notified prior to next event, then start the loop over to
				//account for any possible new task or to respond to termination
				if( m_cv.wait_until(next_time) == std::cv_status::no_timeout)
					continue;

				//Having waited the appropriate time and not received any notifications otherwise
				//Re-retrieve the upcoming task and double-check that it is due. This is really just in case
				//it's possible for a timeout and notification to race, which you would hope not.
				next = m_schedule.begin();
				next_time = next->first;
				next_task = next->second;
				if(clock_type::now() < next_time)
					continue;

				//Ok, so it's really time to run this task
				//It shouldn't be possible for the task to have been removed because
				//we would have received a no_timeout instead, looped again, and retrieved a valid item
				try{
					next_task();
					m_schedule.erase(next);
				}
				catch(...){
					m_schedule.erase(next);
					m_error_handler( std::current_exception() );
				}
			}
			else
				m_cv.wait( lock );
		}
	}

public:
	using iterator = typename map_type::iterator;

	Scheduler(){
		m_thread = { thread_proc, this };
	}

	~Scheduler(){
		{
			auto lock = lock_type(m_mutex);
			m_terminate = true;
			m_cv.notify_one();
		}
		if(m_thread.joinable())
			m_thread.join();
	}

	iterator schedule( const time_type &time, task_type &&task ){
		auto lock = lock_type(m_mutex);
		auto result = m_schedule.insert( {time, std::move(task) } );
		m_cv.notify_one();
		return result;
	}

	void cancel( const iterator &it ){
		auto lock = lock_type(m_mutex);
		m_schedule.erase(it);
		m_cv.notify_one();
	}

};

