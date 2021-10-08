#pragma once
#include <string>
#include <fstream>
#include <stdexcept>
#include <map>
#include "util.hpp"
#include "binary_file.hpp"

namespace k3yab::bawns{

namespace state_file_blocks{

class StateFileError:public std::runtime_error{
	using base_type = std::runtime_error;

public:
	using base_type::base_type;
};

template<typename T>
using file_pos = levitator::util::binfile::FilePosition<T>;

class callsign_type{
	char m_callsign[16];

public:
	callsign_type();
	callsign_type(const std::string &);
	void verify() const;
};

struct record_start{
	char start = '[';
	void verify() const;
};

struct record_end{
	char end = ']';
	void verify() const;
};

template<typename T>
void check_record_ends( const T &rec ){
	rec.rstart.verify();
	rec.rend.verify();
}

struct node{	
	record_start rstart;
	callsign_type callsign;							//Callsign "XXXXXXX-YY\0" (etc)
	file_pos<levitator::util::binfile::blocks::linked_list<node>> link_list;			//First link in a linked list of nodes found reachable from this one
	int query_count = 0;							//Number of times the node has been explored to completion, may be zero
	record_end rend;

	void verify() const;
};

#define STATE_FILE_HEADER_ID "W00T"

struct state_file_header{

	static constexpr char identifier_string[] = STATE_FILE_HEADER_ID;
	static constexpr int current_file_version = 1;

	record_start rstart;
	char identifier[ sizeof(identifier_string) ] = STATE_FILE_HEADER_ID;
	int endian_stamp = 1;
	int file_version = current_file_version;
	file_pos<levitator::util::binfile::blocks::linked_list<node>> root_node_list;
	record_end rend;

	void verify() const;
};

}

namespace state{

class StateFile{
	using node_map_type = std::map<std::string, levitator::util::binfile::FilePosition<state_file_blocks::node>>;

	template<typename T>
	using pos_type = levitator::util::binfile::FilePosition<T>;

	//static constexpr pos_type<state_file_blocks::state_file_header> header_pos = {0};

	std::fstream m_stream;
	levitator::util::binfile::BinaryFile m_bfile;
	node_map_type m_root_nodes, m_nodes, m_pending_nodes;

public:
	StateFile( const std::filesystem::path &path );

};


}
}
