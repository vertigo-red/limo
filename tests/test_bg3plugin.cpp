#include "../src/core/bg3plugin.h"
#include <catch2/catch_test_macros.hpp>


/*!
 * A minimal meta.lsx in the shape Bg3Plugin::Bg3Plugin parses: a <save> node
 * containing an id="Config" node with a root/children tree holding ModuleInfo
 * (Name, UUID, Version, Description, Folder) and Dependencies entries.
 */
namespace
{
const std::string plugin_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<save>
  <version major="4"/>
  <region id="ModuleSettings">
    <node id="Config">
      <children>
        <node id="root">
          <children>
            <node id="ModuleInfo">
              <attribute id="Name" value="My Mod"/>
              <attribute id="Folder" value="MyModFolder"/>
              <attribute id="Version" value="16777216"/>
              <attribute id="UUID" value="11111111-2222-3333-4444-555555555555"/>
              <attribute id="Description" value="A test plugin"/>
            </node>
            <node id="Dependencies">
              <children>
                <node id="ModuleShortDesc">
                  <attribute id="Folder" type="LSString" value="DepOne"/>
                  <attribute id="Name" type="LSString" value="Dependency One"/>
                  <attribute id="UUID" type="guid"
                             value="aaaaaaaa-0000-0000-0000-000000000001"/>
                </node>
                <node id="ModuleShortDesc">
                  <attribute id="Folder" type="LSString" value="DepTwo"/>
                  <attribute id="Name" type="LSString" value="Dependency Two"/>
                  <attribute id="UUID" type="guid"
                             value="bbbbbbbb-0000-0000-0000-000000000002"/>
                </node>
              </children>
            </node>
          </children>
        </node>
      </children>
    </node>
  </region>
</save>)";
}  // namespace


TEST_CASE("Bg3Plugin parses module info from meta.lsx", "[bg3plugin]")
{
  const Bg3Plugin plugin(plugin_xml);
  REQUIRE(plugin.getName() == "My Mod");
  REQUIRE(plugin.getUuid() == "11111111-2222-3333-4444-555555555555");
  REQUIRE(plugin.getVersion() == "16777216");
  REQUIRE(plugin.getDirectory() == "MyModFolder");
  REQUIRE(plugin.getDescription() == "A test plugin");
  REQUIRE(plugin.getDependencies().size() == 2);
}

TEST_CASE("Bg3Plugin exposes its raw xml and detects known dependencies", "[bg3plugin]")
{
  Bg3Plugin plugin(plugin_xml);
  REQUIRE(plugin.getXmlString() == plugin_xml);

  REQUIRE(plugin.hasDependency("aaaaaaaa-0000-0000-0000-000000000001"));
  REQUIRE(plugin.hasDependency("bbbbbbbb-0000-0000-0000-000000000002"));
  REQUIRE_FALSE(plugin.hasDependency("ffffffff-0000-0000-0000-0000000000ff"));
}

TEST_CASE("Bg3Plugin computes missing dependencies", "[bg3plugin]")
{
  Bg3Plugin plugin(plugin_xml);
  const std::vector<std::pair<std::string, std::string>> empty_missing =
    plugin.getMissingDependencies(
      { "aaaaaaaa-0000-0000-0000-000000000001", "bbbbbbbb-0000-0000-0000-000000000002" });
  REQUIRE(empty_missing.empty());

  const auto missing = plugin.getMissingDependencies(
    { "aaaaaaaa-0000-0000-0000-000000000001" });
  REQUIRE(missing.size() == 1);
  REQUIRE(missing[0].first == "bbbbbbbb-0000-0000-0000-000000000002");
  REQUIRE(missing[0].second == "Dependency Two");
}

TEST_CASE("Bg3Plugin emits xml plugin and loadorder strings", "[bg3plugin]")
{
  const Bg3Plugin plugin(plugin_xml);
  const std::string plugin_string = plugin.toXmlPluginString();
  REQUIRE(plugin_string.find("My Mod") != std::string::npos);
  REQUIRE(plugin_string.find("MyModFolder") != std::string::npos);
  REQUIRE(plugin_string.find("11111111-2222-3333-4444-555555555555") != std::string::npos);
  REQUIRE(plugin_string.find("16777216") != std::string::npos);

  const std::string order_string = plugin.toXmlLoadorderString();
  REQUIRE(order_string.find("11111111-2222-3333-4444-555555555555") != std::string::npos);
}

TEST_CASE("Bg3Plugin writes into Mods and ModOrder nodes", "[bg3plugin]")
{
  const Bg3Plugin plugin(plugin_xml);

  pugi::xml_document mods_doc;
  auto mods_node = mods_doc.append_child("node");
  mods_node.append_attribute("id") = "Mods";
  plugin.addToXmlModsNode(mods_node);

  const auto added = mods_node.find_child_by_attribute("id", "ModuleShortDesc");
  REQUIRE_FALSE(added.empty());
  REQUIRE(added.child("attribute").child_attribute("value").value() ==
          plugin.getDirectory());

  pugi::xml_document order_doc;
  auto order_node = order_doc.append_child("node");
  order_node.append_attribute("id") = "ModOrder";
  plugin.addToXmlOrderNode(order_node);

  const auto ordered = order_node.find_child_by_attribute("id", "Module");
  REQUIRE_FALSE(ordered.empty());
  REQUIRE(ordered.child("attribute").child_attribute("value").value() ==
          plugin.getUuid());
}

TEST_CASE("Bg3Plugin validates plugins and rejects vanilla", "[bg3plugin]")
{
  REQUIRE(Bg3Plugin::isValidPlugin(plugin_xml));

  std::string vanilla = plugin_xml;
  const auto pos = vanilla.find("11111111-2222-3333-4444-555555555555");
  REQUIRE(pos != std::string::npos);
  vanilla.replace(pos,
                  std::string("11111111-2222-3333-4444-555555555555").size(),
                  "28ac9ce2-2aba-8cda-b3b5-6e922f71b6b8");
  REQUIRE_FALSE(Bg3Plugin::isValidPlugin(vanilla));

  REQUIRE_FALSE(Bg3Plugin::isValidPlugin("<not-a-plugin/>"));
}
