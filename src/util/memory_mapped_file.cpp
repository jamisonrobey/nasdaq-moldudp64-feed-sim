#include "memory_mapped_file.h"

#include <fcntl.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace util
{
    MemoryMappedFile::MemoryMappedFile(const std::filesystem::path& path, int flags, off_t offset)
    {
        const int fd{open(path.c_str(), O_RDONLY)};
        if (fd < 0)
        {
            throw std::system_error(errno, std::system_category());
        }

        fd_ = fd;
        length_ = std::filesystem::file_size(path);

        mapped_file_ = mmap(nullptr, length_, PROT_READ, flags, fd_, offset);
        if (mapped_file_ == MAP_FAILED)
        {
            throw std::system_error(errno, std::system_category());
        }
    }

    MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
        : fd_{std::exchange(other.fd_, -1)},
          length_{std::exchange(other.length_, 0)},
          mapped_file_{std::exchange(other.mapped_file_, nullptr)} {}

    MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            fd_ = std::exchange(other.fd_, -1);
            length_ = std::exchange(other.length_, 0);
            mapped_file_ = std::exchange(other.mapped_file_, nullptr);
        }
        return *this;
    }

    MemoryMappedFile::~MemoryMappedFile()
    {
        cleanup();
    }

    std::span<const char> MemoryMappedFile::as_span() const noexcept
    {
        return std::span(static_cast<char*>(mapped_file_), length_);
    }

    void MemoryMappedFile::cleanup() noexcept
    {
        if (mapped_file_ != nullptr)
        {
            munmap(mapped_file_, length_);
        }
        if (fd_ >= 0)
        {
            close(fd_);
        }
    }
}
