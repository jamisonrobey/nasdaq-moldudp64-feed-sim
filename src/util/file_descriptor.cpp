#include "file_descriptor.h"

#include <fcntl.h>
#include <stdexcept>
#include <utility>
#include <unistd.h>

namespace util
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
        const int fd{open(path.c_str(), flags)};
        if (fd < 0)
        {
            throw std::system_error(errno, std::system_category());
        }
        fd_ = fd;
    }

    FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept
        : fd_{std::exchange(other.fd_, -1)} {}

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
