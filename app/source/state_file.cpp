#include <cstring>
#include "meta.hpp"
#include "util.hpp"
#include "state_file.hpp"

using namespace k3yab::bawns;
using namespace k3yab::bawns::state_file_blocks;

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

void state_file_blocks::record_start::verify() const{
	if(start != '[')
		throw StateFileError("State file framing error. Start of record not found.");
}

void state_file_blocks::record_end::verify() const{
	if(end != ']')
		throw StateFileError("State file framing error. End of record not found.");
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

state::StateFile::StateFile( const std::filesystem::path &path ):
	m_stream(path, m_stream.binary | m_stream.in | m_stream.out ),
	m_bfile(m_stream, 4096){

	//New/empty file case
	if(m_bfile.size() == 0){
		m_bfile.alloc<state_file_blocks::header>();
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