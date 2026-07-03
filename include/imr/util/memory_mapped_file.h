#pragma once

#include "imr/util/file_descriptor.h"

#include <sys/mman.h>
#include <span>

namespace imr::util
{
    /** RAII mmap file

     Read only so PROT_READ and only returns const span view
     */
    class MemoryMappedFile
    {
      public:
        struct Config
        {
            /**
             Path of the file to map

             @throws std::invalid_argument if invalid path or directory
             @throws std::system_error if open() fails
            */
            std::filesystem::path path;
            /**
             Additional mmap() flags, OR'd with MAP_PRIVATE.
             
             MAP_PRIVATE is always used since the mapping is read only, but you can pass additional flags via this.
             */
            int mmap_flags{0};
            /// Flags passed to madvise() after mapping. Pass 0 to skip madvise() call entirely.
            int madvise_flags{0};
        };

        explicit MemoryMappedFile(const Config& cfg);

        MemoryMappedFile(const MemoryMappedFile&) = delete;
        MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

        MemoryMappedFile(MemoryMappedFile&& other) noexcept;
        MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

        ~MemoryMappedFile();

        [[nodiscard]]
        std::span<const char> as_span() const noexcept;

      private:
        FileDescriptor fd_;
        std::size_t length_{0};
        void* mapped_file_{nullptr};

        void cleanup() noexcept;
    };
}
