#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include <unistd.h>

namespace test_common
{
    template <typename Derived>
    class TestFileFixture : public ::testing::Test
    {
      protected:
        static std::filesystem::path test_path()
        {
            // return path as "'suite name'_pid" so we don't use same file path for tests if we run the suite in parallel
            static const std::filesystem::path path{[] {
                const auto* suite_info{::testing::UnitTest::GetInstance()->current_test_suite()};
                const auto filename{std::string(suite_info->name()) + "_" + std::to_string(getpid())};
                return std::filesystem::path(TEST_DATA_DIR) / filename;
            }()};
            return path;
        }

        static void SetUpTestSuite()
        {
            std::ofstream out(test_path(), std::ios::binary | std::ios::trunc);
            ASSERT_TRUE(out) << "Failed to create temporary test file: " << test_path();
            const auto content{Derived::get_test_content()};
            out.write(content.data(), content.size());
        }

        static void TearDownTestSuite()
        {
            std::filesystem::remove(test_path());
        }
    };
}
