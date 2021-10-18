#include <iostream>
#include "binary_file.hpp"

using namespace levitator::binfile;

levitator::binfile::BinaryFile::AppendGuard::AppendGuard(BinaryFile &f):
	m_bf(&f),
	m_lock( f.make_lock() ),
	m_position( f.size() ){}

levitator::binfile::BinaryFile::AppendGuard::~AppendGuard(){
	if(m_bf && m_bf->size() > m_position)
		m_bf->resize( m_position );
}

void levitator::binfile::BinaryFile::AppendGuard::release(){
	m_bf = nullptr;
}

levitator::binfile::BinaryFile::BinaryFile():
	m_state{ nullptr }{}

levitator::binfile::BinaryFile::BinaryFile( std::iostream &stream, std::streamsize initial_size ):m_state{ &stream }{
	//:m_file(stream){	

	stream.exceptions( std::iostream::badbit | std::iostream::failbit | std::iostream::eofbit );
	auto sz = size_on_disk();	

	//New/empty file case
	if(sz == 0)
		m_state.cache.reserve(initial_size);
	else{
		//Read the entire file image into cache vector
		m_state.file->read( &m_state.cache.front(), sz );
	}
}

//Commit the file image back to disk
levitator::binfile::BinaryFile::~BinaryFile(){
	if(m_state.file)
		flush();	
}

BinaryFile &BinaryFile::operator=(BinaryFile &&rhs){
	m_state = std::move(rhs.m_state);
	return *this;
}

void levitator::binfile::BinaryFile::flush(){
	auto lock = make_lock();
	m_state.file->write( &m_state.cache.front(), m_state.cache.size() );
	m_state.file->flush();	
}

BinaryFile::size_type BinaryFile::size() const{
	auto lock = make_lock();
	return m_state.cache.size();
}

//levitator::binfile::BinaryFile::file_ref_type levitator::binfile::BinaryFile::get(){
//	return { m_file, m_mutex };
//}

std::streamsize levitator::binfile::BinaryFile::size_on_disk_impl() const{	
	m_state.file->seekg(0 , std::ios_base::end);
	return m_state.file->tellg();
}

std::streamsize levitator::binfile::BinaryFile::size_on_disk() const{
	auto lock = make_lock();
	auto oldpos = m_state.file->tellg();
	auto result = size_on_disk_impl();
	m_state.file->seekg(oldpos, std::ios_base::cur);
	return result;
}

