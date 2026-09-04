#include "../src/core/installer.h"
#include "test_utils.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <iostream>
#include <vector>


TEST_CASE("Extraction rejects path traversal entries", "[installer][security]")
{
  resetStagingDir();
  const sfs::path malicious_archive = DATA_DIR / "staging" / "malicious.tar";

  sfs::path entry_names[] = { "../escaped.txt", "/abs_escape.txt" };
  size_t escaped = 0;
  {
    struct archive* a = archive_write_new();
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, malicious_archive.string().c_str());
    for(const auto& name : entry_names)
    {
      struct archive_entry* e = archive_entry_new();
      archive_entry_set_pathname(e, name.string().c_str());
      archive_entry_set_filetype(e, AE_IFREG);
      archive_entry_set_perm(e, 0644);
      archive_write_header(a, e);
      archive_write_data(a, "payload", 7);
      archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
  }

  try
  {
    Installer::extract(malicious_archive, DATA_DIR / "staging" / "extract");
  }
  catch(const std::exception&)
  {
    // Either a throw (secure rejection) or a confined write is acceptable; the
    // security invariant is that nothing lands outside the destination dir.
  }

  if(sfs::exists(DATA_DIR / "staging" / "escaped.txt"))
    escaped++;
  if(sfs::exists(DATA_DIR / "escaped.txt"))
    escaped++;
  if(sfs::exists(DATA_DIR / "abs_escape.txt"))
    escaped++;
  REQUIRE(escaped == 0);

  sfs::remove_all(malicious_archive);
  if(sfs::exists(DATA_DIR / "staging"))
    sfs::remove_all(DATA_DIR / "staging");
}

TEST_CASE("Files are extracted", "[installer]")
{
  resetStagingDir();
  Installer::extract(DATA_DIR / "source" / "mod0.tar.gz", DATA_DIR / "staging" / "extract");
  verifyDirsAreEqual(DATA_DIR / "source" / "0", DATA_DIR / "staging" / "extract");
}

TEST_CASE("Mods are (un)installed", "[installer]")
{
  resetStagingDir();
  Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                     DATA_DIR / "staging",
                     Installer::preserve_case | Installer::preserve_directories);

  SECTION("Simple installer")
  verifyDirsAreEqual(DATA_DIR / "source/0", DATA_DIR / "staging");
  SECTION("Uninstallation")
  {
    Installer::uninstall(DATA_DIR / "staging", Installer::SIMPLEINSTALLER);
    REQUIRE_FALSE(sfs::exists(DATA_DIR / "staging"));
  }
}

TEST_CASE("Installer options", "[installer]")
{
  resetStagingDir();
  SECTION("Upper case conversion")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "upper",
                       Installer::upper_case | Installer::preserve_directories);
    verifyDirsAreEqual(DATA_DIR / "target" / "upper", DATA_DIR / "staging" / "upper");
  }
  SECTION("lower case conversion")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "lower",
                       Installer::lower_case | Installer::preserve_directories);
    verifyDirsAreEqual(DATA_DIR / "target" / "lower", DATA_DIR / "staging" / "lower");
  }
  SECTION("Single directory")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "single_dir",
                       Installer::preserve_case | Installer::single_directory);
    verifyDirsAreEqual(DATA_DIR / "target" / "single_dir", DATA_DIR / "staging" / "single_dir");
  }
  SECTION("Upper case and single directory")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "upper_single",
                       Installer::upper_case | Installer::single_directory);
    verifyDirsAreEqual(DATA_DIR / "target" / "upper_single", DATA_DIR / "staging" / "upper_single");
  }
}

TEST_CASE("Root levels", "[installer]")
{
  resetStagingDir();
  SECTION("Level 0")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "0",
                       Installer::preserve_case | Installer::preserve_directories,
                       Installer::SIMPLEINSTALLER,
                       0);
    verifyDirsAreEqual(DATA_DIR / "target" / "root_level" / "0", DATA_DIR / "staging" / "0");
  }
  SECTION("Level 1")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "1",
                       Installer::preserve_case | Installer::preserve_directories,
                       Installer::SIMPLEINSTALLER,
                       1);
    verifyDirsAreEqual(DATA_DIR / "target" / "root_level" / "1", DATA_DIR / "staging" / "1");
  }
  SECTION("Level 2")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "2",
                       Installer::preserve_case | Installer::preserve_directories,
                       Installer::SIMPLEINSTALLER,
                       2);
    verifyDirsAreEqual(DATA_DIR / "target" / "root_level" / "2", DATA_DIR / "staging" / "2");
  }
  SECTION("Level 3")
  {
    Installer::install(DATA_DIR / "source" / "mod0.tar.gz",
                       DATA_DIR / "staging" / "3",
                       Installer::preserve_case | Installer::preserve_directories,
                       Installer::SIMPLEINSTALLER,
                       3);
    verifyDirsAreEqual(DATA_DIR / "target" / "root_level" / "3", DATA_DIR / "staging" / "3");
  }
}

TEST_CASE("File names are marked as directories only for directories", "[installer]")
{
  const auto entries =
    Installer::getArchiveFileNames(DATA_DIR / "source" / "revdepl" / "data" / "1");
  const auto find_entry = [&entries](const sfs::path& name)
  {
    return std::find_if(entries.begin(),
                        entries.end(),
                        [&name](const auto& p) { return p.first == name; });
  };

  const auto some_file = find_entry("some file.txt");
  REQUIRE(some_file != entries.end());
  REQUIRE_FALSE(some_file->second);
  REQUIRE_FALSE(find_entry("12")->second);

  const auto a_dir = find_entry("a");
  REQUIRE(a_dir != entries.end());
  REQUIRE(a_dir->second);
  REQUIRE_FALSE(find_entry(sfs::path("a") / "1")->second);
  REQUIRE(find_entry(sfs::path("b") / "5") != entries.end());
}
