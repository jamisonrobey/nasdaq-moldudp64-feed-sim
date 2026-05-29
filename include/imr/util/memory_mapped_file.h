#pragma once

#include "imr/util/file_descriptor.h"

#include <sys/mman.h>

namespace imr::util
{
    class MemoryMappedFile
    {
      public:
        struct Config
        {
            // this throws if not a valid path or is a directory
            std::filesystem::path path;
            // MAP_PRIVATE always used cause read only but you can add more with this
            int mmap_flags{0};
            off_t offset{0};
            // 0 skips madvise
            int madvise_flags{0};
        };

        explicit MemoryMappedFile(const Config& cfg);

        MemoryMappedFile(const MemoryMappedFile&) = delete;
        MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;

        MemoryMappedFile(MemoryMappedFile&& other) noexcept;
        MemoryMappedFile& operator=(MemoryMappedFile&& other) noexcept;

        ~MemoryMappedFile();

        // only require read only access
        [[nodiscard]]
        std::span<const char> as_span() const noexcept;

      private:
        FileDescriptor fd_;
        std::size_t length_{0};
        void* mapped_file_{nullptr};

        void cleanup() noexcept;
    };
}
