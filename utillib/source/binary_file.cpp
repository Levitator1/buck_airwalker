#include <iostream>
#include "binary_file.hpp"

using namespace levitator::binfile;

levitator::binfile::AppendGuard::AppendGuard(BinaryFile &f):
	m_bf(&f),
	m_position(f.size()){}

levitator::binfile::AppendGuard::~AppendGuard(){
	if(m_bf && m_bf->size() > m_position)
		m_bf->resize( m_position );
}

void levitator::binfile::AppendGuard::release(){
	m_bf = nullptr;
}

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
	if(m_file)
		flush();	
}

void levitator::binfile::BinaryFile::flush(){
	m_file.write( &m_cache.front(), m_cache.size() );
	m_file.flush();	
}

BinaryFile::size_type BinaryFile::size() const{
	return m_cache.size();
}

levitator::binfile::BinaryFile::file_ref_type levitator::binfile::BinaryFile::get(){
	return { m_file, m_mutex };
}

std::streamsize levitator::binfile::BinaryFile::size_on_disk_impl() const{	
	m_file.seekg(0 , std::ios_base::end);
	return m_file.tellg();
}

std::streamsize levitator::binfile::BinaryFile::size_on_disk() const{
	auto lock = make_lock();
	auto oldpos = m_file.tellg();
	auto result = size_on_disk_impl();
	m_file.seekg(oldpos, std::ios_base::cur);
	return result;
}