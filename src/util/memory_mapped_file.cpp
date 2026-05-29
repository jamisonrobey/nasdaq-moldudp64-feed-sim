#include "imr/util/memory_mapped_file.h"

#include <filesystem>
#include <sys/mman.h>
#include <system_error>
#include <utility>
#include <source_location>

namespace imr::util
{
    MemoryMappedFile::MemoryMappedFile(const Config& cfg)
        : fd_(cfg.path),
          length_{std::filesystem::file_size(cfg.path)}
    {
        // we only read file ever so PROT_READ and MAP_PRIVATE always
        mapped_file_ = mmap(nullptr, length_, PROT_READ, MAP_PRIVATE | cfg.mmap_flags, fd_.get(), cfg.offset);
        if (mapped_file_ == MAP_FAILED)
        {
            throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
        }

        if (cfg.madvise_flags == 0)
        {
            return;
        }

        if (madvise(mapped_file_, std::filesystem::file_size(cfg.path), cfg.madvise_flags) == -1)
        {
            throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
        }
    }

    MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other) noexcept
        : fd_{std::move(other.fd_)},
          length_{std::exchange(other.length_, 0)},
          mapped_file_{std::exchange(other.mapped_file_, nullptr)} {}

    MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other) noexcept
    {
        if (this != &other)
        {
            cleanup();
            fd_ = std::move(other.fd_);
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
    }
}
