#pragma once
#include <iostream>
#include <mutex>
#include "concurrency/locked_ostream_proxy.hpp"

/*
* Just a central place to put console messaging in, mainly for the purposes of exclusive locking,
* so that threads don't talk over each other.
*/

namespace jab::util{

class Console{

	using mutex_type = std::recursive_mutex;
	mutex_type m_in_mutex, m_out_mutex, m_err_mutex;
	std::istream *m_inp = nullptr;
	std::ostream *m_outp = nullptr, *m_errp = nullptr;

public:
	using in_type = levitator::concurrency::locked_istream_ref<std::istream, mutex_type>;
	using out_type = levitator::concurrency::locked_ostream_ref<std::ostream, mutex_type>;
	using err_type = out_type;

	//Requires an explicit call to init in order to avoid static fiasco in pre-main
	void init();
	in_type in();
	out_type out();
	err_type err();
};

Console console;

}