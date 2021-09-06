#pragma once
#include <filesystem>
#include "File.hpp"

namespace jab::file{

//A file opened via a path in the filesystem
class FSFile:public File{
public:
    FSFile() = default;
    FSFile(const std::filesystem::path &path, int fl);
};

}