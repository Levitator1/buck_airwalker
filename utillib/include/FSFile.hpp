#pragma once
#include "Config.hpp"
#include <filesystem>
#include "File.hpp"

namespace jab::file{

//A file opened via a path in the filesystem
class FSFile:public File{
public:
    FSFile() = default;
    FSFile(const std::filesystem::path &path, int fl);
};

template<typename Char = char, typename Traits = std::char_traits<Char>>
class FSFile_iostream:public File_iostream<Char, FSFile, Traits>{
	using base_type = File_iostream<Char, FSFile, Traits>;

public:
	using base_type::base_type;

	FSFile_iostream(const std::filesystem::path &path, int fl, std::streamsize bufsize = jab::util::Config::io_block_size):
		base_type( {path, fl}, bufsize ){
	}
};

}
