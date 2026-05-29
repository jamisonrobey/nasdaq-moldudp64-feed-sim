#include <gtest/gtest.h>

#include "imr/util/memory_mapped_file.h"

#include <sys/mman.h>
#include <stdexcept>
#include <string_view>

using namespace imr::util;

namespace
{
    constexpr std::string_view mmap_file_path{TEST_DATA_DIR "/mmap_test_file.txt"};
}

TEST(MemoryMappedFile, Ctor_MinimalConfig_NoThrow)
{
    EXPECT_NO_THROW(MemoryMappedFile({
        .path = mmap_file_path,
    }));
}

TEST(MemoryMappedFile, Ctor_FullConfig_NoThrow)
{
    EXPECT_NO_THROW(MemoryMappedFile({
        .path = mmap_file_path,
        .mmap_flags = MAP_POPULATE,
        .offset = 0,
        .madvise_flags = MADV_SEQUENTIAL,
    }));
}

TEST(MemoryMappedFile, Ctor_BadPath_ThrowsInvalidArgument)
{
    // empty
    EXPECT_THROW(MemoryMappedFile({}), std::invalid_argument);
    // non existent
    EXPECT_THROW(MemoryMappedFile({.path = "asdf"}), std::invalid_argument);
    // directory
    EXPECT_THROW(MemoryMappedFile({.path = TEST_DATA_DIR}), std::invalid_argument);
}
