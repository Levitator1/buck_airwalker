#include "binary_file.hpp"

using namespace levitator::binfile;

levitator::binfile::BinaryFile::BinaryFile( std::iostream &stream, std::streamsize initial_size ):
	m_file(stream){

	stream.exceptions( std::iostream::badbit | std::iostream::failbit | std::iostream::eofbit );
	auto sz = size_on_disk();	

	//New/empty file case
	if(sz == 0)
		m_cache.reserve(initial_size);
	else{
		//Read the entire file image into cache vector
		m_file.read( &m_cache.front(), sz );
	}
}

//Commit the file image back to disk
levitator::binfile::BinaryFile::~BinaryFile(){
	m_file.write( &m_cache.front(), m_cache.size() );
	m_file.flush();
}

BinaryFile::size_type BinaryFile::size() const{
	return m_cache.size();
}

levitator::binfile::BinaryFile::file_ref_type levitator::binfile::BinaryFile::get(){
	return { m_file, m_mutex };
}

std::streampos levitator::binfile::BinaryFile::size_on_disk(){
	auto lock = std::lock_guard(m_mutex);
	m_file.seekp(0 , std::ios_base::end);
	return m_file.tellp();
}
