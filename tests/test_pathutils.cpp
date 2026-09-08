#include "../src/core/pathutils.h"
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace sfs = std::filesystem;
namespace pu = path_utils;


namespace
{
std::filesystem::path makeUniqueDir(const std::string& base)
{
  static std::atomic<unsigned> counter{ 0 };
  return std::filesystem::temp_directory_path() /
         ("lmm_pathutils_test_" + base + "_" + std::to_string(++counter));
}

void touch(const sfs::path& file)
{
  std::ofstream(file) << "content";
}

/*! RAII work directory that always cleans up after itself. */
struct WorkDir
{
  sfs::path root;
  explicit WorkDir(const std::string& base) : root(makeUniqueDir(base))
  {
    sfs::create_directories(root);
  }
  ~WorkDir()
  {
    std::error_code ec;
    sfs::remove_all(root, ec);
  }
};
}  // namespace


TEST_CASE("path_utils normalizes and lowercases paths", "[pathutils]")
{
  REQUIRE(pu::normalizePath("a\\b\\c") == "a/b/c");
  REQUIRE(pu::normalizePath("a/b/c") == "a/b/c");
  REQUIRE(pu::normalizePath("") == "");

  REQUIRE(pu::toLowerCase(sfs::path("My/MiXeD.PaTh")) == "my/mixed.path");
}

TEST_CASE("path_utils computes relative paths and lengths", "[pathutils]")
{
  REQUIRE(pu::getRelativePath(sfs::path("/a/b/c.txt"), sfs::path("/a/b")) == "c.txt");
  REQUIRE(pu::getRelativePath(sfs::path("/a/b"), sfs::path("/a/b")) == "");
  REQUIRE(pu::getRelativePath(sfs::path("/a/b/d/e.txt"), sfs::path("/a/b")) == "d/e.txt");

  REQUIRE(pu::getPathLength(sfs::path("a/b/c")) == 3);
  REQUIRE(pu::getPathLength(sfs::path("a")) == 1);
}

TEST_CASE("path_utils removes leading path components", "[pathutils]")
{
  const auto [head, short_path] = pu::removePathComponents(sfs::path("a/b/c/d"), 2);
  REQUIRE(head == sfs::path("a/b"));
  REQUIRE(short_path == sfs::path("c/d"));

  const auto [head_zero, short_zero] = pu::removePathComponents(sfs::path("a/b"), 0);
  REQUIRE(head_zero == sfs::path());
  REQUIRE(short_zero == sfs::path("a/b"));
}

TEST_CASE("path_utils moves files into a directory", "[pathutils]")
{
  WorkDir wd("move");
  const auto source = wd.root / "source";
  const auto destination = wd.root / "destination";
  sfs::create_directories(source / "sub");
  touch(source / "a.txt");
  touch(source / "sub" / "b.txt");

  pu::moveFilesToDirectory(source, destination);

  REQUIRE(sfs::exists(destination / "a.txt"));
  REQUIRE(sfs::exists(destination / "sub" / "b.txt"));
  REQUIRE_FALSE(sfs::exists(source));
}

TEST_CASE("path_utils copies files into a directory", "[pathutils]")
{
  WorkDir wd("copy");
  const auto source = wd.root / "source";
  const auto destination = wd.root / "destination";
  sfs::create_directories(source);
  touch(source / "a.txt");

  pu::moveFilesToDirectory(source, destination, false);

  REQUIRE(sfs::exists(destination / "a.txt"));
  REQUIRE(sfs::exists(source));
}

TEST_CASE("path_utils renameFiles renames files recursively", "[pathutils]")
{
  WorkDir wd("rename");
  const auto source = wd.root / "source";
  const auto destination = wd.root / "destination";
  sfs::create_directories(source);
  touch(source / "abc.txt");

  const auto to_upper = [](unsigned char c) -> unsigned char {
    return static_cast<unsigned char>(std::toupper(c));
  };
  pu::renameFiles(destination, source, to_upper);

  REQUIRE(sfs::exists(destination / "ABC.TXT"));
  REQUIRE_FALSE(sfs::exists(destination / "abc.txt"));
}

TEST_CASE("path_utils moveFilesWithDepth flattens directory levels", "[pathutils]")
{
  WorkDir wd("depth");
  const auto source = wd.root / "source";
  const auto destination = wd.root / "destination";
  sfs::create_directories(source / "one" / "two");
  touch(source / "one" / "two" / "deep.txt");

  pu::moveFilesWithDepth(source, destination, 1);

  REQUIRE(sfs::exists(destination / "two" / "deep.txt"));
  REQUIRE_FALSE(sfs::exists(source));
}

TEST_CASE("path_utils detects empty directories", "[pathutils]")
{
  WorkDir wd("empty");
  const auto empty_dir = wd.root / "empty_dir";
  const auto only_subdirs = wd.root / "only_subdirs";
  const auto with_file = wd.root / "with_file";
  sfs::create_directories(empty_dir);
  sfs::create_directories(only_subdirs / "sub");
  sfs::create_directories(with_file);
  touch(with_file / "file.txt");

  REQUIRE(pu::directoryIsEmpty(empty_dir));
  REQUIRE(pu::directoryIsEmpty(only_subdirs));
  REQUIRE_FALSE(pu::directoryIsEmpty(with_file));
  REQUIRE(pu::directoryIsEmpty(with_file, { "file.txt" }));
  REQUIRE_FALSE(pu::directoryIsEmpty(empty_dir / "missing"));
}

TEST_CASE("path_utils exists treats symlinks and missing paths correctly", "[pathutils]")
{
  WorkDir wd("exists");
  const auto file = wd.root / "file.txt";
  touch(file);

  REQUIRE(pu::exists(file));
  REQUIRE_FALSE(pu::exists(wd.root / "missing.txt"));
  REQUIRE_FALSE(pu::exists(wd.root / "missing" / "nested.txt"));
}

TEST_CASE("path_utils finds case-insensitive path variants", "[pathutils]")
{
  WorkDir wd("case");
  const auto dir = wd.root / "MixedCase";
  sfs::create_directories(dir);
  touch(dir / "MyFile.TXT");

  const auto found =
    pu::pathExists(sfs::path("mixedcase/myfile.txt"), wd.root);
  REQUIRE(found.has_value());
  REQUIRE(*found == sfs::path("MixedCase/MyFile.TXT"));

  const auto exact = pu::pathExists(sfs::path("MixedCase/MyFile.TXT"), wd.root);
  REQUIRE(exact.has_value());

  const auto sensitive =
    pu::pathExists(sfs::path("mixedcase/MyFile.TXT"), wd.root, false);
  REQUIRE_FALSE(sensitive.has_value());

  const auto missing = pu::pathExists(sfs::path("not_there"), wd.root);
  REQUIRE_FALSE(missing.has_value());
}