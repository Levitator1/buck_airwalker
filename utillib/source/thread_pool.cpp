#include <memory>
#include <vector>
#include <iostream>
#include "exception.hpp"
#include "concurrency/thread_pool.hpp"

using namespace levitator::concurrency;
using namespace jab::exception;

DefaultThreadPoolExceptionHandler::DefaultThreadPoolExceptionHandler():
	base_type( message ){
}