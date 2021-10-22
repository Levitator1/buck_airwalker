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

	if( std::string( identifier, sizeof(identifier)-1 ) != std::string(identifier_string) )
		throw StateFileError("State file identifier does not match. Looks like the wrong format.");

	if( endian_stamp != 1 )
		throw StateFileError("State file endian stamp is wrong. Maybe this state file is from an other-endian machine.");

	if( file_version != current_file_version )
		throw StateFileError("State file version numbers don't match");
}

void state::StateFile::insert_all_nodes_node( node_type &nd){
	auto call = std::string(nd.callsign.c_str());
	auto ptr = offset_ptr<node>(&nd, &m_state.bfile );

	//TODO
	//auto value = typename node_map_type::value_type(call, ptr );
	//auto result = m_state.nodes.insert( value );
	//if(!result.second)
	//	throw StateFileError("There's a duplicate entry in the state file, which means it's corrupt: " + nd.callsign.str());
}

std::fstream null_stream;

//state::StateFile::StateFile():
//	m_stream(),
//	m_bfile(m_stream, 0){}

static std::fstream open_file( const std::filesystem::path &path ){
	//Open momentarily for writing only to ensure that the file is created
	//if it does not yet exist
	{
		std::fstream tmp = { path,  std::fstream::out | std::fstream::ate | std::fstream::app};
	}
	return {path, std::fstream::binary | std::fstream::in | std::fstream::out };
} 

state::StateFile::StateFile( const std::filesystem::path &path ):m_state{
	open_file(path),
	{m_state.stream, 4096},
	{path}}{

	//New/empty file case
	if(m_state.bfile.size_on_disk() == 0){
		m_state.bfile.construct<state_file_blocks::header>();
	}
	else{
		header().get().verify();
		//No nodes to process
		if(!header().get().all_nodes.next)
			return;

		//Post-process an existing file with possible nodes in it
		//First, build a dictionary of all of the node callsigns so that duplicates can be caught
		//and while we are at it, we will build a list of those which are incomplete and need visiting.
		for(auto &node : m_state.file_nodes){
			node.verify();
			insert_all_nodes_node(node);

			if(node.query_count < header().get().visit_serial)
				m_state.pending.push_back( {&node, &m_state.bfile });
		}
	}
}

state::StateFile::~StateFile(){

	if(!m_state.bfile)
		return;

	m_state.bfile.flush();

	const auto sz = m_state.bfile.size();
	const auto dsz = m_state.bfile.size_on_disk();

	//Probably a poor convention to close a file which was provided to us open
	//But it has to be resized
	//m_state.stream.close();

	//Somewhat awkwardly reopen the file to truncate it if it shrank, which it usually won't
	if( sz < dsz ){
		FSFile tmp(m_state.file_path, w);
		tmp.truncate(sz);
	}
}

state::StateFile &state::StateFile::operator=( state::StateFile &&rhs ){
	m_state = std::move(rhs.m_state);
	m_state.bfile.file(m_state.stream);
	return *this;
}

template<typename BF, class = jab::meta::permit_any_cv<BF, levitator::binfile::BinaryFile>>
static BinaryFile::locked_ref< typename jab::util::copy_cv<BF, header>::type> fetch_header( BF &bf ){
	//Lock twice, because we are creating a locked reference for something that could get relocated before the locked reference is created
	auto lock = bf.make_lock();
	return bf.make_lock( *bf.template fetch<header>(0) );
}

BinaryFile::locked_ref<header> state::StateFile::header(){
	return fetch_header(m_state.bfile);
}

std::size_t state::StateFile::size() const{
	return m_state.nodes.size();
}

BinaryFile::locked_ref<const header> state::StateFile::header() const{
	return fetch_header(m_state.bfile);
}

state::StateFile::const_iterator_type state::StateFile::begin() const{
	auto guard = m_state.bfile.make_lock();
	//return header::node_list_type::cbegin( &*header().get().all_node_listp ).lock_rebound( std::move(guard) );
	auto it =  this->m_state.file_nodes.cbegin();
	return it.lock( std::move(guard) );
}

/*
state::StateFile::const_iterator_type state::StateFile::end() const{
	auto guard = m_state.bfile.make_lock();
	return header::node_list_type::cend( &*header().get().all_node_listp ).lock_rebound( std::move(guard) );
}

state::StateFile::iterator_type state::StateFile::begin(){
	auto guard = m_state.bfile.make_lock();
	return header::node_list_type::begin( &*header().get().all_node_listp ).lock_rebound( std::move(guard) );
}

state::StateFile::iterator_type state::StateFile::end(){
	auto guard = m_state.bfile.make_lock();
	return header::node_list_type::end( &*header().get().all_node_listp ).lock_rebound( std::move(guard) );
}
*/

state::StateFile::node_pointer_type state::StateFile::append_node(const std::string &callsign){

	//Update the state file
	auto lock = m_state.bfile.make_lock();

	//auto nodep = m_bfile.alloc<state_file_blocks::node>(callsign);
	//auto linkp = m_bfile.alloc<header::node_list_type>( nodep, header().all_node_listp);
	//header().all_node_listp = linkp;
	//auto nodep = m_state.bfile.list_insert<state_file_blocks::node>( header().get().all_node_listp, callsign );
	auto &node = m_state.file_nodes.push_front(  {callsign} );

	//Update table of all nodes
	insert_all_nodes_node(node);

	//Remember that this node has not been visited
	m_state.pending.push_back( {&node, &m_state.bfile} );
	return {&node};
}

/*
state::StateFile::node_pointer_type state::StateFile::append_root_node(const std::string &callsign){	
	//Don't guard here because then you wind up with a list pointing to nothing
	auto result = append_node(callsign);		
}
*/
