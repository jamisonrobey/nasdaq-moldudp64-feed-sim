#include <gtest/gtest.h>
#include <test_file_fixture.h>

#include "imr/util/memory_mapped_file.h"

#include <sys/mman.h>
#include <stdexcept>
#include <string_view>
#include <span>
#include <algorithm>

using namespace imr::util;

namespace
{
    class MemoryMappedFileTest : public test_common::TestFileFixture<MemoryMappedFileTest>
    {
      public:
        static constexpr std::string_view get_test_content()
        {
            return expected_content;
        }

      protected:
        static constexpr std::string_view expected_content{"hello mmap file"};
    };
}

TEST_F(MemoryMappedFileTest, Ctor_MinimalConfig_VerifyContents)
{
    MemoryMappedFile file({
        .path = test_path(),
    });

    const auto file_span{file.as_span()};

    ASSERT_EQ(file_span.size(), expected_content.size());

    const std::span<const char> mapped_data{file_span.data(), file_span.size()};
    EXPECT_TRUE(std::ranges::equal(mapped_data, expected_content));
}

TEST_F(MemoryMappedFileTest, Ctor_FullConfig_VerifyContents)
{
    MemoryMappedFile file({
        .path = test_path(),
        .mmap_flags = MAP_POPULATE,
        .madvise_flags = MADV_SEQUENTIAL,
    });

    const auto file_span{file.as_span()};

    ASSERT_EQ(file.as_span().size(), expected_content.size());

    const std::span<const char> mapped_data{file_span.data(), file_span.size()};
    EXPECT_TRUE(std::ranges::equal(mapped_data, expected_content));
}

TEST_F(MemoryMappedFileTest, Ctor_BadPath_ThrowsInvalidArgument)
{
    // empty
    EXPECT_THROW(MemoryMappedFile({}), std::invalid_argument);
    // non existent
    EXPECT_THROW(MemoryMappedFile({.path = "asdf"}), std::invalid_argument);
    // directory
    EXPECT_THROW(MemoryMappedFile({.path = TEST_DATA_DIR}), std::invalid_argument);
}
