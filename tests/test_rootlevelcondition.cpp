#include "../src/ui/rootlevelcondition.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <json/json.h>
#include <QString>
#include <QTreeWidgetItem>
#include <memory>
#include <optional>
#include <stdexcept>


namespace
{
QTreeWidgetItem* addChild(QTreeWidgetItem* parent, const char* text, bool is_directory)
{
  auto* child = new QTreeWidgetItem(parent);
  child->setText(0, QString::fromUtf8(text));
  child->setData(0, Qt::UserRole, is_directory);
  return child;
}

QTreeWidgetItem* makeDefaultTree()
{
  auto* root = new QTreeWidgetItem();
  root->setText(0, "root");
  root->setData(0, Qt::UserRole, true);

  auto* data = addChild(root, "Data", true);
  auto* meshes = addChild(data, "meshes", true);
  addChild(meshes, "model.nif", false);
  auto* textures = addChild(data, "textures", true);
  addChild(textures, "tex.dds", false);

  addChild(root, "plugin.esp", false);
  addChild(root, "README.txt", false);
  return root;
}
} // namespace

TEST_CASE("Wildcard matching finds a file at the root level", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());
  const RootLevelCondition condition(RootLevelCondition::simple,
                                     RootLevelCondition::file,
                                     false,
                                     true,
                                     "*.esp",
                                     0);
  REQUIRE(condition.detectRootLevel(root.get()) == std::optional<int>(0));
}

TEST_CASE("Nested matches report their depth and apply the level offset", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());
  const RootLevelCondition condition(RootLevelCondition::simple,
                                     RootLevelCondition::file,
                                     false,
                                     true,
                                     "*.dds",
                                     0);
  REQUIRE(condition.detectRootLevel(root.get()) == std::optional<int>(2));

  const RootLevelCondition offset_condition(RootLevelCondition::simple,
                                            RootLevelCondition::file,
                                            false,
                                            true,
                                            "*.dds",
                                            1);
  REQUIRE(offset_condition.detectRootLevel(root.get()) == std::optional<int>(1));
}

TEST_CASE("Directories can be matched as match targets", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());
  const RootLevelCondition condition(RootLevelCondition::simple,
                                     RootLevelCondition::directory,
                                     false,
                                     true,
                                     "meshes",
                                     0);
  REQUIRE(condition.detectRootLevel(root.get()) == std::optional<int>(1));
}

TEST_CASE("Regex matching works", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());
  const RootLevelCondition condition(
    RootLevelCondition::regex, RootLevelCondition::any, false, true, ".*\\.dds", 0);
  REQUIRE(condition.detectRootLevel(root.get()) == std::optional<int>(2));
}

TEST_CASE("Case-invariant matching ignores case", "[rootlevel]")
{
  // Only the target text is lowercased; the wildcard matcher itself is case-sensitive.
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());

  const RootLevelCondition unnormalized_expression(
    RootLevelCondition::simple, RootLevelCondition::file, true, true, "*.ESP", 0);
  REQUIRE_FALSE(unnormalized_expression.detectRootLevel(root.get()).has_value());

  auto* upper_root = new QTreeWidgetItem();
  upper_root->setText(0, "root");
  upper_root->setData(0, Qt::UserRole, true);
  addChild(upper_root, "PLUGIN.ESP", false);
  std::unique_ptr<QTreeWidgetItem> root2(upper_root);

  const RootLevelCondition invariant(
    RootLevelCondition::simple, RootLevelCondition::file, true, true, "*.esp", 0);
  REQUIRE(invariant.detectRootLevel(root2.get()) == std::optional<int>(0));

  const RootLevelCondition sensitive(
    RootLevelCondition::simple, RootLevelCondition::file, false, true, "*.esp", 0);
  REQUIRE_FALSE(sensitive.detectRootLevel(root2.get()).has_value());
}

TEST_CASE("Target filters restrict which items are matched", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());

  const RootLevelCondition files_only(
    RootLevelCondition::simple, RootLevelCondition::directory, false, true, "*.esp", 0);
  REQUIRE_FALSE(files_only.detectRootLevel(root.get()).has_value());

  const RootLevelCondition anything(
    RootLevelCondition::simple, RootLevelCondition::any, false, true, "*", 0);
  REQUIRE(anything.detectRootLevel(root.get()) == std::optional<int>(0));
}

TEST_CASE("Missing matches return no result", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());
  const RootLevelCondition condition(RootLevelCondition::simple,
                                     RootLevelCondition::file,
                                     false,
                                     true,
                                     "*.nomatch",
                                     0);
  REQUIRE_FALSE(condition.detectRootLevel(root.get()).has_value());
}

TEST_CASE("RootLevelCondition is deserialized from JSON", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());

  Json::Value json;
  json["matcher_type"] = "simple";
  json["target_type"] = "file";
  json["expression"] = "*.esp";
  json["case_invariant"] = true;
  json["level_offset"] = 1;

  const RootLevelCondition condition(json);
  REQUIRE(condition.detectRootLevel(root.get()) == std::optional<int>(-1));
}

TEST_CASE("RootLevelCondition JSON defaults are applied", "[rootlevel]")
{
  std::unique_ptr<QTreeWidgetItem> root(makeDefaultTree());

  Json::Value json;
  json["matcher_type"] = "simple";
  json["target_type"] = "any";
  json["expression"] = "*";

  const RootLevelCondition condition(json);
  REQUIRE(condition.detectRootLevel(root.get()) == std::optional<int>(0));
}

TEST_CASE("RootLevelCondition JSON rejects missing keys", "[rootlevel]")
{
  Json::Value json;
  json["matcher_type"] = "simple";
  json["target_type"] = "file";
  REQUIRE_THROWS_WITH(RootLevelCondition(json),
                      Catch::Matchers::ContainsSubstring("Missing json key: 'expression'."));
}

TEST_CASE("RootLevelCondition JSON rejects unknown matcher types", "[rootlevel]")
{
  Json::Value json;
  json["matcher_type"] = "bogus";
  json["target_type"] = "file";
  json["expression"] = "*.esp";
  REQUIRE_THROWS_WITH(RootLevelCondition(json),
                      Catch::Matchers::ContainsSubstring("Invalid matcher type: 'bogus'."));
}