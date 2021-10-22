#pragma once
#include <string>
#include <fstream>
#include <stdexcept>
#include <list>
#include <map>
#include "util.hpp"
#include "concurrency/thread_pool.hpp"
#include "binary_file.hpp"

namespace k3yab::bawns{

namespace state_file_blocks{

template<typename T>
using file_ptr = levitator::binfile::blocks::RelPtr<T>;

class StateFileError:public std::runtime_error{
	using base_type = std::runtime_error;

public:
	using base_type::base_type;
};

class callsign_type{
	char m_callsign[16];	//Must be null-terminated, so max 15 chars

public:
	callsign_type();
	callsign_type(const std::string &);
	void verify() const;
	const char *c_str() const;
	std::string str() const;
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
	const callsign_type callsign;												//Callsign "XXXXXXX-YY\0" (etc)
	
	file_ptr<levitator::binfile::blocks::linked_list<node>> link_list;			//First link in a linked list of nodes found reachable from this one	
	int query_count = 0;														//Number of times the node has been explored to completion, may be zero
	record_end rend;

	node( const std::string &callsign );
	void verify() const;
};

#define STATE_FILE_HEADER_ID "W00T"

struct header{
	using node_list_type = levitator::binfile::blocks::linked_list<node>;
	//using node_list_pointer_type = file_ptr<node_list_type>;

	static constexpr char identifier_string[] = STATE_FILE_HEADER_ID;
	static constexpr int current_file_version = 1;

	record_start rstart;
	char identifier[ sizeof(identifier_string) ] = STATE_FILE_HEADER_ID;
	int endian_stamp = 1;
	int file_version = current_file_version;
	int visit_serial = 1;	//A serial number to discern which nodes have been visited
							//nodes with a lesser visit number are considered to need visiting
	node_list_type all_nodes, root_nodes;
	record_end rend;

	void verify() const;
};

}

namespace state{

namespace impl{

	struct StateOffsetPtrBaseFunc{
		levitator::binfile::BinaryFile *file;
		
		char *operator()() const{
			return file->fetch<char>(0);
		}
	};

	//This is non-copyable for some obscure reason I lack the time or energy to fuck around with.	
	/*
	inline auto make_binfile_base_func( levitator::binfile::BinaryFile *file ){
		return [file](){ return file->fetch<char>(0); };
	}
	using make_binfile_base_func_type = decltype( make_binfile_base_func( nullptr ) );
	*/
}

//For pointing from outside the data file into it
template<typename T>
class StateOffsetPtr:public levitator::binfile::blocks::OffsetPtr<T, impl::StateOffsetPtrBaseFunc>{
	using base_type = levitator::binfile::blocks::OffsetPtr<T, impl::StateOffsetPtrBaseFunc>;
	using pointer_type = typename base_type::pointer_type;

public:
	template<typename U>
	using rebind = StateOffsetPtr<U>;

	//This pointer's not relative to its location, so you can just copy the rhs state to make a copy
	StateOffsetPtr( const StateOffsetPtr &rhs ):
		base_type(rhs){
	}

	StateOffsetPtr(pointer_type ptr, levitator::binfile::BinaryFile *file):
		base_type(ptr, { file } ){}

	StateOffsetPtr(std::nullptr_t = {} ):
		StateOffsetPtr( pointer_type(nullptr), nullptr ){}

	//Construct rebound from another pointer to a different T
	template<class U>
	StateOffsetPtr( pointer_type ptr, const StateOffsetPtr<U> &rebind_from):
		StateOffsetPtr(ptr, rebind_from.m_base_f){
	}

	StateOffsetPtr &operator=( const StateOffsetPtr &rhs ){
		base_type::operator=(rhs);
		return *this;
	}

	StateOffsetPtr &operator=(  pointer_type ptr){
		base_type::operator=(ptr);
		return *this;
	}
};

template<typename Ptr>
StateOffsetPtr( Ptr,  levitator::binfile::BinaryFile &) -> StateOffsetPtr< typename std::pointer_traits<Ptr>::element_type >;

template<typename Ptr, typename U>
StateOffsetPtr( Ptr,  const StateOffsetPtr<U> &) -> StateOffsetPtr< typename std::pointer_traits<Ptr>::element_type >;

class StateFile{
	template<typename T>
	using offset_ptr = StateOffsetPtr<T>;
	using header_type = state_file_blocks::header;
	using node_type = state_file_blocks::node;
	using node_map_type = std::map<std::string, offset_ptr<node_type>>;	
	using node_pointer_type = state_file_blocks::file_ptr<node_type>;
	using node_list_type = state_file_blocks::header::node_list_type;
	using BinaryFile = levitator::binfile::BinaryFile;
	using node_list_view_type = levitator::binfile::linked_list_view< offset_ptr< typename node_list_type::link_type >, BinaryFile::allocator<node_type> >;		

	struct State{
		std::fstream stream;
		BinaryFile bfile;
		std::filesystem::path file_path;
		node_list_view_type file_nodes;	//All nodes, as stored in the file
		node_map_type nodes;			//In-memory index of nodes, indexed by callsign
		std::list<offset_ptr<node_type>> pending;		
	} m_state;

	void insert_all_nodes_node(node_type &n);

public:
	using iterator_type = decltype( std::declval<typename node_list_view_type::iterator_type>().lock( std::declval<BinaryFile::locked_ref<>>() ) );
	using const_iterator_type = decltype( std::declval<typename node_list_view_type::const_iterator_type>().lock( std::declval<BinaryFile::locked_ref<const BinaryFile>>() ) );
	//using iterator_type = state_file_blocks::header::node_list_type::iterator_type::rebind_for_lock< BinaryFile::locked_ref<> >;
	//using const_iterator_type = state_file_blocks::header::node_list_type::const_iterator_type::rebind_for_lock< BinaryFile::const_locked_ref<> >;

	//StateFile();	//uninitialized and invalid state file
	StateFile() = default;
	StateFile( const std::filesystem::path & );
	~StateFile();
	StateFile &operator=( StateFile && );
	//auto pending = jab::util::range_property( [this](){ return this->m_state.pending.begin(); }, [this](){ return this->m_state.pending.end(); });
	std::size_t size() const;
	const_iterator_type begin() const;
	const_iterator_type end() const;
	iterator_type begin();
	iterator_type end();
	BinaryFile::locked_ref<state_file_blocks::header> header();
	BinaryFile::locked_ref<const state_file_blocks::header> header() const;
	node_pointer_type append_node(const std::string &callsign);
	node_pointer_type append_root_node( const std::string &callsign );

	auto pending_nodes() const{
		return jab::util::range_property( 
			[this](){ return this->m_state.pending.begin(); }, 
			[this](){ return this->m_state.pending.end(); });
	}

};

/*
class StateFile{
	using node_map_type = std::map<std::string, levitator::binfile::FilePosition<state_file_blocks::node>>;

	template<typename T>
	using pos_type = levitator::binfile::FilePosition<T>;

	//static constexpr pos_type<state_file_blocks::header> header_pos = {0};

	std::fstream m_stream;
	levitator::binfile::BinaryFile m_bfile;
	node_map_type m_root_nodes, m_nodes, m_pending_nodes;

public:
	StateFile( const std::filesystem::path &path );

};
*/


}
}
