#include "binary_file.hpp"

using namespace levitator::util;

levitator::util::binfile::BinaryFile::BinaryFile( std::iostream &stream ):
	m_file(stream){		
}

levitator::util::binfile::BinaryFile::~BinaryFile(){
	m_file.flush();
}

levitator::util::binfile::BinaryFile::file_ref_type levitator::util::binfile::BinaryFile::get(){
	return { m_file, m_mutex };
}
