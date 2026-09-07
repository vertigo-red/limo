#include "../src/core/lspakextractor.h"
#include "test_utils.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>


namespace
{
void writeMinimalPak(const sfs::path& pak_path, uint32_t num_files, uint32_t compressed_size)
{
  LsPakHeader header{};
  header.magic_number = 0x4b50534c; // "LSPK"
  header.version = 18;
  header.file_list_offset = sizeof(LsPakHeader);
  header.file_list_size = 8; // only the count and compressed size are written
  header.flags = 0;
  header.priority = 0;
  std::memset(header.md5, 0, sizeof(header.md5));
  header.num_parts = 1;

  std::ofstream file(pak_path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(&header), sizeof(header));
  file.write(reinterpret_cast<const char*>(&num_files), sizeof(num_files));
  file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
}
} // namespace

TEST_CASE("File lists exceeding the 1 GiB cap are rejected", "[lspak]")
{
  // sizeof(LsPakFileListEntry) * num_files would exceed the 1 GiB guard and
  // previously either truncated the buffer size or caused a huge parse loop.
  const sfs::path pak_path = sfs::temp_directory_path() / "limo_oversized_file_list.pak";
  writeMinimalPak(pak_path, 10000000, 0);

  LsPakExtractor extractor(pak_path);
  REQUIRE_THROWS_WITH(extractor.init(),
                      Catch::Matchers::ContainsSubstring("File list is too large"));

  sfs::remove(pak_path);
}

TEST_CASE("Bounded file lists are not rejected by the size guard", "[lspak]")
{
  const sfs::path pak_path = sfs::temp_directory_path() / "limo_bounded_file_list.pak";
  writeMinimalPak(pak_path, 1, 0);

  LsPakExtractor extractor(pak_path);
  try
  {
    extractor.init();
    FAIL("init() should not succeed for a file list with no data");
  }
  catch(const std::runtime_error& error)
  {
    REQUIRE(std::string(error.what()).find("File list is too large") == std::string::npos);
  }

  sfs::remove(pak_path);
}