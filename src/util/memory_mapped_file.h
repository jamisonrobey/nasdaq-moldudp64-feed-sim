#pragma once

#include "util/file_descriptor.h"
#include <filesystem>
#include <sys/mman.h>

namespace util
{
    class MemoryMappedFile
    {
      public:
        explicit MemoryMappedFile(const std::filesystem::path& path, int flags = MAP_PRIVATE, off_t offset = 0);

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
