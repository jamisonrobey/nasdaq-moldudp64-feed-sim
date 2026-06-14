#pragma once

#include <gtest/gtest.h>

#include "test_file.h"

#include <filesystem>
#include <fstream>

namespace test_common
{
    template <std::size_t NumMessages, std::int64_t TimestampIncrementNs = 1>
    class TestFileFixture : public ::testing::Test
    {
      protected:
        static constexpr std::string_view test_path{TEST_DATA_DIR "/test_file"};

        static void SetUpTestSuite()
        {
            static constexpr std::array test_file{create_test_file<NumMessages>(std::chrono::nanoseconds(TimestampIncrementNs))};
            std::ofstream out(std::filesystem::path(test_path), std::ios::binary | std::ios::trunc);
            ASSERT_TRUE(out) << "Failed to create test file";
            out.write(test_file.data(), test_file.size());
        }

        static void TearDownTestSuite()
        {
            std::filesystem::remove(test_path);
        }
    };
}
