#pragma once

#include <filesystem>
#include <fcntl.h>

namespace util
{
    class FileDescriptor
    {
      public:
        explicit FileDescriptor(int fd);
        explicit FileDescriptor(const std::filesystem::path& path, int flags = O_RDONLY);

        FileDescriptor(const FileDescriptor&) = delete;
        FileDescriptor& operator=(const FileDescriptor&) = delete;

        FileDescriptor(FileDescriptor&& other) noexcept;
        FileDescriptor& operator=(FileDescriptor&& other) noexcept;

        ~FileDescriptor();

        [[nodiscard]]
        int get() const noexcept;

      private:
        int fd_{-1};

        void cleanup() const noexcept;
    };
}
