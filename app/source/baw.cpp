#include "util.hpp"
#include "concurrency/thread_pool.hpp"
#include "state_file.hpp"
#include "console.hpp"
#include "baw.hpp"

using namespace std;
using namespace k3yab::bawns;
using namespace levitator::concurrency;
using namespace jab::util;

baw_exception::baw_exception(const string &msg):
    runtime_error(msg){}

k3yab::bawns::baw::baw(const k3yab::bawns::Config &conf):
	m_config(conf){		
}


void k3yab::bawns::baw::run(){
	using thread_pool_type = ThreadPool< std::unique_ptr<node_task>, PointerPoolThread<std::unique_ptr<node_task>>  >;

	console.out() << "Starting..." << endl;
	thread_pool_type workers(m_config.threads);

	console.out() << "Using state file: " << m_config.state_path << endl;
	m_state = { m_config.state_path };
}


int k3yab::bawns::node_task::operator()(){
	return 0;
}

