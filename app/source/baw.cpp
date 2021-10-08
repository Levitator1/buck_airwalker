#include "concurrency/thread_pool.hpp"
#include "state_file.hpp"
#include "baw.hpp"

using namespace std;
using namespace k3yab::bawns;
using namespace levitator::concurrency;

baw_exception::baw_exception(const string &msg):
    runtime_error(msg){}

void k3yab::bawns::baw::run(){
	ThreadPool workers(m_config.threads);
}

