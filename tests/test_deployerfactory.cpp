#include "../src/core/deployerfactory.h"
#include "../src/core/bg3deployer.h"
#include "../src/core/casematchingdeployer.h"
#include "../src/core/lootdeployer.h"
#include "../src/core/openmwarchivedeployer.h"
#include "../src/core/openmwplugindeployer.h"
#include "../src/core/reversedeployer.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <typeinfo>


namespace sfs = std::filesystem;

namespace
{
const sfs::path SOURCE_PATH = sfs::temp_directory_path() / "lmm_factory_test_source";
const sfs::path DEST_PATH = sfs::temp_directory_path() / "lmm_factory_test_dest";

template <typename Leaf>
bool isExact(const std::unique_ptr<Deployer>& deployer)
{
  return typeid(*deployer) == typeid(Leaf);
}
}  // namespace


TEST_CASE("DeployerFactory constructs every deployer type", "[deployerfactory]")
{
  for(const auto& type : DeployerFactory::DEPLOYER_TYPES)
    REQUIRE_NOTHROW(
      DeployerFactory::makeDeployer(type, SOURCE_PATH, DEST_PATH, "factory-test"));
}

TEST_CASE("DeployerFactory creates the concrete deployer class", "[deployerfactory]")
{
  auto simple =
    DeployerFactory::makeDeployer(DeployerFactory::SIMPLEDEPLOYER, SOURCE_PATH, DEST_PATH, "s");
  auto case_match = DeployerFactory::makeDeployer(
    DeployerFactory::CASEMATCHINGDEPLOYER, SOURCE_PATH, DEST_PATH, "c");
  auto loot =
    DeployerFactory::makeDeployer(DeployerFactory::LOOTDEPLOYER, SOURCE_PATH, DEST_PATH, "l");
  auto reverse =
    DeployerFactory::makeDeployer(DeployerFactory::REVERSEDEPLOYER, SOURCE_PATH, DEST_PATH, "r");
  auto openmw_archive = DeployerFactory::makeDeployer(
    DeployerFactory::OPENMWARCHIVEDEPLOYER, SOURCE_PATH, DEST_PATH, "a");
  auto openmw_plugin = DeployerFactory::makeDeployer(
    DeployerFactory::OPENMWPLUGINDEPLOYER, SOURCE_PATH, DEST_PATH, "p");
  auto bg3 =
    DeployerFactory::makeDeployer(DeployerFactory::BG3DEPLOYER, SOURCE_PATH, DEST_PATH, "b");

  REQUIRE(isExact<Deployer>(simple));
  REQUIRE(isExact<CaseMatchingDeployer>(case_match));
  REQUIRE(isExact<LootDeployer>(loot));
  REQUIRE(isExact<ReverseDeployer>(reverse));
  REQUIRE(isExact<OpenMwArchiveDeployer>(openmw_archive));
  REQUIRE(isExact<OpenMwPluginDeployer>(openmw_plugin));
  REQUIRE(isExact<Bg3Deployer>(bg3));
}

TEST_CASE("DeployerFactory passes name, paths and modes through", "[deployerfactory]")
{
  auto deployer = DeployerFactory::makeDeployer(
    DeployerFactory::CASEMATCHINGDEPLOYER, SOURCE_PATH, DEST_PATH, "my-deployer", Deployer::copy);

  REQUIRE(deployer->getName() == "my-deployer");
  REQUIRE(deployer->getSourcePath() == SOURCE_PATH);
  REQUIRE(deployer->getDestPath() == DEST_PATH);
}

TEST_CASE("DeployerFactory rejects unknown deployer types", "[deployerfactory]")
{
  REQUIRE_THROWS_AS(DeployerFactory::makeDeployer(
                      "Not A Deployer", SOURCE_PATH, DEST_PATH, "x"),
                    std::runtime_error);
}