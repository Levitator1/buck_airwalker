#include <cstring>
#include <utility>
#include <string>
#include "meta.hpp"
#include "util.hpp"
#include "state_file.hpp"
#include "FSFile.hpp"		//Needed just for truncating files

using namespace k3yab::bawns;
using namespace k3yab::bawns::state_file_blocks;
using namespace jab::file;
using namespace levitator::binfile;

state_file_blocks::callsign_type::callsign_type(){
	m_callsign[0] = '\0';
}

state_file_blocks::callsign_type::callsign_type( const std::string &str ){
	if(str.size() >= sizeof(m_callsign))
		throw std::invalid_argument("Callsign too long");
	
	std::copy(str.begin(), str.end(), m_callsign);
	m_callsign[ str.size() ] = '\0';
}

void state_file_blocks::callsign_type::callsign_type::verify() const{
	try{
		jab::util::strnlenlt( m_callsign, sizeof(m_callsign) );
	}
	catch(...){
		std::throw_with_nested( StateFileError("Unterminated callsign in state file.") );
	}
}

const char *state_file_blocks::callsign_type::c_str() const{
	return m_callsign;
}

std::string state_file_blocks::callsign_type::str() const{
	return c_str();
}

void state_file_blocks::record_start::verify() const{
	if(start != '[')
		throw StateFileError("State file framing error. Start of record not found.");
}

void state_file_blocks::record_end::verify() const{
	if(end != ']')
		throw StateFileError("State file framing error. End of record not found.");
}

state_file_blocks::node::node( const std::string &csign ):
	callsign(csign){

}

void state_file_blocks::node::verify() const{
	check_record_ends(*this);	
	callsign.verify();
}

void state_file_blocks::header::verify() const{
	check_record_ends(*this);

	if( std::string( identifier, sizeof(identifier) ) != std::string(identifier_string) )
		throw StateFileError("State file identifier does not match. Looks like the wrong format.");

	if( endian_stamp != 1 )
		throw StateFileError("State file endian stamp is wrong. Maybe this state file is from an other-endian machine.");

	if( file_version != current_file_version )
		throw StateFileError("State file version numbers don't match");
}

void state::StateFile::insert_all_nodes_node( node_type &nd){
	auto result = m_nodes.insert( { nd.callsign.c_str() , &nd } );
	if(!result.second)
		throw StateFileError("There's a duplicate entry in the state file, which means it's corrupt: " + node.callsign.str());
}

state::StateFile::StateFile( const std::filesystem::path &path ):
	m_stream(path, m_stream.binary | m_stream.in | m_stream.out ),
	m_bfile(m_stream, 4096),
	m_file_path(path){

	//New/empty file case
	if(m_bfile.size() == 0){
		m_bfile.alloc<state_file_blocks::header>();
	}
	else{
		//No nodes to process
		if(!header().all_node_listp)
			return;

		//Post-process an existing file with possible nodes in it
		//First, build a dictionary of all of the node callsigns so that duplicates can be caught
		//and while we are at it, we will build a list of those which are incomplete and need visiting.
		for(auto &node : *header().all_node_listp ){
			insert_all_nodes_node(node);

			if(node.query_count <= 0)
				m_pending.push_back(&node);
		}
	}
}

state::StateFile::~StateFile(){

	m_bfile.flush();
	m_stream.close();

	//Somewhat awkwardly reopen the file to truncate it if it shrank, which it usually won't
	if( m_bfile.size() < m_bfile.size_on_disk() ){
		FSFile tmp(m_file_path, w);
		tmp.truncate(m_bfile.size());
	}
}

template<typename BF, class = jab::meta::permit_any_cv<BF, levitator::binfile::BinaryFile>>
static auto &fetch_header( BF &bf ){
	return *bf.template fetch<k3yab::bawns::state_file_blocks::header>(0);
}

state_file_blocks::header &state::StateFile::header(){
	return fetch_header(m_bfile);
}

const state_file_blocks::header &state::StateFile::header() const{
	return fetch_header(m_bfile);
}

state::StateFile::const_iterator_type state::StateFile::begin() const{
	return header::node_list_type::cbegin( &*header().all_node_listp );
}

state::StateFile::const_iterator_type state::StateFile::end() const{
	return header::node_list_type::cend( &*header().all_node_listp );
}

state::StateFile::node_pointer_type state::StateFile::append_node(const std::string &callsign){

	//Update the state file
	AppendGuard guard(m_bfile);
	auto nodep = m_bfile.alloc<state_file_blocks::node>(callsign);
	auto linkp = m_bfile.alloc<header::node_list_type>( nodep, header().all_node_listp);
	header().all_node_listp = linkp;

	//Update table of all nodes
	insert_all_nodes_node(*nodep);

	//Remember that this node has not been visited
	m_pending.push_back(nodep);
	
	return nodep;
}

