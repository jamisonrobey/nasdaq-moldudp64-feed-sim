#include "imr/util/file_descriptor.h"

#include <fcntl.h>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <unistd.h>
#include <format>

namespace imr::util
{
    FileDescriptor::FileDescriptor(int fd)
        : fd_{fd}
    {
        if (fd_ < 0)
        {
            throw std::invalid_argument("util::FileDescriptor given negative fd");
        }
    }

    FileDescriptor::FileDescriptor(const std::filesystem::path& path, int flags)
    {
        if (!std::filesystem::exists(path) || std::filesystem::is_directory(path))
        {
            throw std::invalid_argument(std::format("{} path is not a file {}",
                                                    std::source_location::current().function_name(),
                                                    path.c_str()));
        }

        const int fd{open(path.c_str(), flags)};
        if (fd < 0)
        {
            throw std::system_error(errno,
                                    std::system_category(),
                                    std::format("{} failed to open file", std::source_location::current().function_name()));
        }

        fd_ = fd;
    }

    FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
        : fd_{std::exchange(other.fd_, -1)}
    {
    }

    FileDescriptor& FileDescriptor::operator=(FileDescriptor&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    FileDescriptor::~FileDescriptor()
    {
        cleanup();
    }

    int FileDescriptor::get() const noexcept
    {
        return fd_;
    }

    void FileDescriptor::cleanup() const noexcept
    {
        if (fd_ >= 0)
        {
            close(fd_);
        }
    }
}
