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

//unique_ptr which forwards the call operator
class callable_task_ptr : public std::unique_ptr<node_task>{
	using base_type = std::unique_ptr<node_task>;

public:
	using base_type::base_type;

	int operator()(){
		return (*this->get())();
	}
};

k3yab::bawns::node_task::node_task(baw &app, const std::string &callsign):
	m_appp(&app),
	m_callsign(callsign){}

k3yab::bawns::node_task::node_task():
	m_appp(nullptr){}

Console::out_type k3yab::bawns::node_task::print() const{	
	return console.out() << m_callsign << ": ";
	//return console.out();
}

int k3yab::bawns::node_task::operator()(){
	if(!m_appp)
		return -1;	
		
	return 0;
}

void k3yab::bawns::baw::run(){
	using thread_pool_type = ThreadPool< node_task  >;

	console.out() << "Starting..." << endl;
	thread_pool_type workers(m_config.threads);

	console.out() << "Using state file: " << m_config.state_path << endl;
	m_state = { m_config.state_path };
	//auto pending_count = m_state.pending.size();	
	//console.out() << "Pending or incomplete nodes from a previous run: " << pending_count << endl;
	console.out() << "Total nodes known: " << m_state.size() << endl;

	//TODO: Previous state resumption goes here (incomplete nodes, etc.)

	console.out() << "Reading stdin for root node callsigns, one per line..." << endl;

	string call;
	int ct = 0;
	while(console.in()){
		std::getline(console.in().get_istream(), call);

		//eof will count as a blank line!
		if(call.size() <= 0)
			continue;

		workers.push( { *this, call } );
		++ct;
	}

	console.out() << ct << " callsigns read. Running query threads..." << endl;
	workers.shutdown();
}
