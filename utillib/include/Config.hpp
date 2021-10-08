#pragma once
#include <stdexcept>
#include <filesystem>
#include "config.h"

namespace jab::util{

struct Config{

	//Systems tend to have a preferred I/O transaction size. 4kB seems to be very common.
	//It's a non-critical number, in any case.
	static constexpr int io_block_size = 4096;
};

}
