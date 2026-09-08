#include "../src/core/deployerfactory.h"
#include "../src/core/bg3deployer.h"
#include "../src/core/casematchingdeployer.h"
#include "../src/core/lootdeployer.h"
#include "../src/core/openmwarchivedeployer.h"
#include "../src/core/openmwplugindeployer.h"
#include "../src/core/reversedeployer.h"
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <typeinfo>


namespace sfs = std::filesystem;

namespace
{
std::filesystem::path makeUniqueDir(const std::string& base)
{
  static std::atomic<unsigned> counter{ 0 };
  return std::filesystem::temp_directory_path() /
         ("lmm_factory_test_" + base + "_" + std::to_string(++counter));
}

template <typename Leaf>
bool isExact(const std::unique_ptr<Deployer>& deployer)
{
  return typeid(*deployer) == typeid(Leaf);
}

/*!
 * Creates src and dest sandbox directories. Some deployer constructors require
 * specific files to exist up front:
 *  - LootDeployer needs a recognizable game file in its source dir.
 *  - OpenMwPluginDeployer reads dest/openmw.cfg while initializing.
 */
struct FactoryDirs
{
  sfs::path source;
  sfs::path dest;
  explicit FactoryDirs(const std::string& base, bool with_game_file = false)
  {
    source = makeUniqueDir(base + "_src");
    dest = makeUniqueDir(base + "_dst");
    sfs::create_directories(source);
    sfs::create_directories(dest);
    if(with_game_file)
      std::ofstream(source / "Fallout4.esm") << "";
  }
  ~FactoryDirs()
  {
    std::error_code ec;
    sfs::remove_all(source, ec);
    sfs::remove_all(dest, ec);
  }
};
}  // namespace


TEST_CASE("DeployerFactory constructs every deployer type", "[deployerfactory]")
{
  FactoryDirs simple("simple");
  FactoryDirs case_match("casematch");
  FactoryDirs loot("loot", /*with_game_file=*/true);
  FactoryDirs reverse("reverse");
  FactoryDirs openmw_archive("openmw_archive");
  FactoryDirs openmw_plugin("openmw_plugin");
  FactoryDirs bg3("bg3");

  // OpenMW plugin deployer needs an openmw.cfg in the target directory.
  std::ofstream(openmw_plugin.dest / "openmw.cfg") << "";

  REQUIRE_NOTHROW(
    DeployerFactory::makeDeployer(DeployerFactory::SIMPLEDEPLOYER, simple.source, simple.dest, "s"));
  REQUIRE_NOTHROW(DeployerFactory::makeDeployer(DeployerFactory::CASEMATCHINGDEPLOYER,
                                               case_match.source,
                                               case_match.dest,
                                               "c"));
  REQUIRE_NOTHROW(
    DeployerFactory::makeDeployer(DeployerFactory::LOOTDEPLOYER, loot.source, loot.dest, "l"));
  REQUIRE_NOTHROW(DeployerFactory::makeDeployer(
    DeployerFactory::REVERSEDEPLOYER, reverse.source, reverse.dest, "r"));
  REQUIRE_NOTHROW(DeployerFactory::makeDeployer(DeployerFactory::OPENMWARCHIVEDEPLOYER,
                                               openmw_archive.source,
                                               openmw_archive.dest,
                                               "a"));
  REQUIRE_NOTHROW(DeployerFactory::makeDeployer(DeployerFactory::OPENMWPLUGINDEPLOYER,
                                               openmw_plugin.source,
                                               openmw_plugin.dest,
                                               "p"));
  REQUIRE_NOTHROW(
    DeployerFactory::makeDeployer(DeployerFactory::BG3DEPLOYER, bg3.source, bg3.dest, "b"));
}

TEST_CASE("DeployerFactory creates the concrete deployer class", "[deployerfactory]")
{
  FactoryDirs simple("simple2");
  FactoryDirs case_match("casematch2");
  FactoryDirs loot("loot2", /*with_game_file=*/true);
  FactoryDirs reverse("reverse2");
  FactoryDirs openmw_archive("openmw_archive2");
  FactoryDirs openmw_plugin("openmw_plugin2");
  FactoryDirs bg3("bg32");
  std::ofstream(openmw_plugin.dest / "openmw.cfg") << "";

  auto simple_ptr = DeployerFactory::makeDeployer(
    DeployerFactory::SIMPLEDEPLOYER, simple.source, simple.dest, "s");
  auto case_ptr = DeployerFactory::makeDeployer(DeployerFactory::CASEMATCHINGDEPLOYER,
                                                case_match.source,
                                                case_match.dest,
                                                "c");
  auto loot_ptr =
    DeployerFactory::makeDeployer(DeployerFactory::LOOTDEPLOYER, loot.source, loot.dest, "l");
  auto reverse_ptr = DeployerFactory::makeDeployer(
    DeployerFactory::REVERSEDEPLOYER, reverse.source, reverse.dest, "r");
  auto archive_ptr = DeployerFactory::makeDeployer(DeployerFactory::OPENMWARCHIVEDEPLOYER,
                                                   openmw_archive.source,
                                                   openmw_archive.dest,
                                                   "a");
  auto plugin_ptr = DeployerFactory::makeDeployer(DeployerFactory::OPENMWPLUGINDEPLOYER,
                                                  openmw_plugin.source,
                                                  openmw_plugin.dest,
                                                  "p");
  auto bg3_ptr =
    DeployerFactory::makeDeployer(DeployerFactory::BG3DEPLOYER, bg3.source, bg3.dest, "b");

  REQUIRE(isExact<Deployer>(simple_ptr));
  REQUIRE(isExact<CaseMatchingDeployer>(case_ptr));
  REQUIRE(isExact<LootDeployer>(loot_ptr));
  REQUIRE(isExact<ReverseDeployer>(reverse_ptr));
  REQUIRE(isExact<OpenMwArchiveDeployer>(archive_ptr));
  REQUIRE(isExact<OpenMwPluginDeployer>(plugin_ptr));
  REQUIRE(isExact<Bg3Deployer>(bg3_ptr));
}

TEST_CASE("DeployerFactory passes name, paths and modes through", "[deployerfactory]")
{
  FactoryDirs dark("passthrough");
  auto deployer = DeployerFactory::makeDeployer(DeployerFactory::CASEMATCHINGDEPLOYER,
                                                dark.source,
                                                dark.dest,
                                                "my-deployer",
                                                Deployer::copy);

  REQUIRE(deployer->getName() == "my-deployer");
  REQUIRE(deployer->getSourcePath() == dark.source);
  REQUIRE(deployer->getDestPath() == dark.dest);
}

TEST_CASE("DeployerFactory rejects unknown deployer types", "[deployerfactory]")
{
  FactoryDirs dark("unknown");
  REQUIRE_THROWS_AS(DeployerFactory::makeDeployer(
                      "Not A Deployer", dark.source, dark.dest, "x"),
                    std::runtime_error);
}