#pragma once

#include <filesystem>
#include <fcntl.h>
#include <source_location>
#include <system_error>
#include <type_traits>
#include <utility>

namespace imr::util
{

    ///  Concept representing any callable taking no arguments and returning an int (wrapping open/dup/socket/etc).
    template <typename Fn>
    concept SyscallInvocable = std::is_invocable_r_v<int, Fn>;

    /// RAII file descriptor wrapper.
    class FileDescriptor
    {
      public:
        FileDescriptor() = default;
        /** Wrap existing descriptor
         *
         * @throws std::invalid_argument if negative
         */
        explicit FileDescriptor(int fd);
        /** Attempt to get descriptor via open()
         *
         * @throws std::invalid_argument if path or directory
         * @throws std::system_error if open() fails
         */
        explicit FileDescriptor(const std::filesystem::path& path, int flags = O_RDONLY);

        /** Constructor for passing any callable that returns a file descriptor.

         @throws std::system_error if the callable returns a negative value.

         @see `SyscallConcept`
        */
        explicit FileDescriptor(SyscallInvocable auto&& syscall)
        {
            fd_ = std::forward<decltype(syscall)>(syscall)();
            if (fd_ < 0)
            {
                throw std::system_error(errno, std::system_category(), std::source_location::current().function_name());
            }
        }

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
