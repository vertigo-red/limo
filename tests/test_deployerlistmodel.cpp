#include "../src/ui/colors.h"
#include "../src/ui/deployerlistmodel.h"
#include "../src/ui/deployerlistproxymodel.h"
#include "../src/ui/modlistmodel.h"
#include <QBrush>
#include <QStringList>
#include <catch2/catch_test_macros.hpp>


namespace
{
class TestDeployerListProxyModel : public DeployerListProxyModel
{
public:
  using DeployerListProxyModel::DeployerListProxyModel;
  void invalidate() { invalidateFilter(); }
};

DeployerInfo threeModInfo()
{
  DeployerInfo info;
  info.mod_names = { "Alpha", "Beta", "Gamma" };
  info.loadorder = { { 1, true }, { 2, false }, { 3, true } };
  info.manual_tags = { { "zzz" }, {}, { "aaa", "bbb" } };
  info.auto_tags = { { "mid" }, {}, {} };
  info.conflict_groups = { { 1 }, { 3 } };
  info.valid_mod_actions = { { 0, 1 }, {}, { 2 } };
  return info;
}
} // namespace

TEST_CASE("An empty deployer list model has four columns and no rows", "[deployerlist]")
{
  DeployerListModel model;
  REQUIRE(model.columnCount() == 4);
  REQUIRE(model.rowCount() == 0);
}

TEST_CASE("The deployer list model exposes the column headers", "[deployerlist]")
{
  DeployerListModel model;
  REQUIRE(model.headerData(DeployerListModel::status_col, Qt::Horizontal).toString() == "Status");
  REQUIRE(model.headerData(DeployerListModel::name_col, Qt::Horizontal).toString() == "Name");
  REQUIRE(model.headerData(DeployerListModel::id_col, Qt::Horizontal).toString() == "ID");
  REQUIRE(model.headerData(DeployerListModel::tags_col, Qt::Horizontal).toString() == "Tags");
  REQUIRE_FALSE(model.headerData(4, Qt::Horizontal).isValid());
  REQUIRE(model.headerData(0, Qt::Vertical).toString() == "1");
  REQUIRE(model.headerData(DeployerListModel::id_col, Qt::Horizontal, Qt::TextAlignmentRole)
          == QVariant(Qt::AlignLeft));
}

TEST_CASE("The deployer list model exposes display data", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());

  REQUIRE(model.rowCount() == 3);
  REQUIRE(model.data(model.index(0, DeployerListModel::status_col)).toString() == "Enabled");
  REQUIRE(model.data(model.index(1, DeployerListModel::status_col)).toString() == "Disabled");
  REQUIRE(model.data(model.index(0, DeployerListModel::name_col)).toString() == "Alpha");
  REQUIRE(model.data(model.index(0, DeployerListModel::id_col)).toInt() == 1);
  REQUIRE(model.data(model.index(0, DeployerListModel::tags_col)).toString() == "mid, zzz");
  REQUIRE(model.data(model.index(1, DeployerListModel::tags_col)).toString().isEmpty());
  REQUIRE(model.data(model.index(2, DeployerListModel::tags_col)).toString() == "aaa, bbb");
}

TEST_CASE("The deployer list model paints status and conflict colors", "[deployerlist]")
{
  DeployerInfo info = threeModInfo();
  info.conflict_groups = { { 1 }, { 2 }, { 3 } };
  DeployerListModel model;
  model.setDeployerInfo(info);

  REQUIRE(model.data(model.index(0, DeployerListModel::status_col), Qt::BackgroundRole)
            .value<QBrush>().color() == colors::GREEN);
  REQUIRE(model.data(model.index(1, DeployerListModel::status_col), Qt::BackgroundRole)
            .value<QBrush>().color() == colors::GRAY);
  REQUIRE(model.data(model.index(0, DeployerListModel::status_col), Qt::ForegroundRole)
            .value<QBrush>().color() == QColor(255, 255, 255));
  REQUIRE(model.data(model.index(0, DeployerListModel::status_col), Qt::TextAlignmentRole)
          == QVariant(Qt::AlignCenter));

  REQUIRE(model.data(model.index(0, DeployerListModel::name_col), Qt::ForegroundRole)
            .value<QBrush>().color() == colors::LIGHT_BLUE);
  REQUIRE(model.data(model.index(1, DeployerListModel::name_col), Qt::ForegroundRole)
            .value<QBrush>().color() == colors::ORANGE);
}

TEST_CASE("The deployer list model exposes custom roles", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());

  REQUIRE(model.data(model.index(1, 0), DeployerListModel::mod_status_role).toBool() == false);
  REQUIRE(model.data(model.index(0, 0), DeployerListModel::mod_id_role).toInt() == 1);
  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_name_role).toString() == "Alpha");
  REQUIRE(model.data(model.index(0, 0), DeployerListModel::mod_tags_role).toStringList()
          == QStringList({ "zzz", "mid" }));
  REQUIRE_FALSE(model.data(model.index(0, 0), DeployerListModel::ids_are_source_references_role).toBool());
  REQUIRE(model.data(model.index(0, 0), DeployerListModel::source_mod_name_role).toString() == "Alpha");
  REQUIRE(model.data(model.index(0, 0), DeployerListModel::valid_mod_actions_role)
            .value<std::vector<int>>() == std::vector<int>{ 0, 1 });
}

TEST_CASE("Source references change the id column", "[deployerlist]")
{
  DeployerInfo info;
  info.mod_names = { "Mod A", "Mod B" };
  info.loadorder = { { 7, true }, { -1, true } };
  info.ids_are_source_references = true;
  info.source_mod_names_ = { "Base Unpacker", "Sourceless" };
  DeployerListModel model;
  model.setDeployerInfo(info);

  REQUIRE(model.headerData(DeployerListModel::id_col, Qt::Horizontal).toString() == "Source Mod");
  REQUIRE(model.data(model.index(0, DeployerListModel::id_col)).toString() == "Base Unpacker [7]");
  REQUIRE(model.data(model.index(1, DeployerListModel::id_col)).toString() == "Sourceless");
  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_id_role).toInt() == 0);
  REQUIRE(model.data(model.index(1, 0), ModListModel::mod_id_role).toInt() == 1);
  REQUIRE(model.data(model.index(0, 0), DeployerListModel::source_mod_name_role).toString()
          == "Base Unpacker");
}

TEST_CASE("The deployer list model reports reverse deployer flags", "[deployerlist]")
{
  DeployerInfo info;
  info.separate_profile_dirs = true;
  info.has_ignored_files = true;
  info.uses_unsafe_sorting = true;
  DeployerListModel model;
  model.setDeployerInfo(info);

  REQUIRE(model.hasSeparateDirs());
  REQUIRE(model.hasIgnoredFiles());
  REQUIRE(model.usesUnsafeSorting());

  DeployerListModel defaults;
  defaults.setDeployerInfo(DeployerInfo{});
  REQUIRE_FALSE(defaults.hasSeparateDirs());
  REQUIRE_FALSE(defaults.hasIgnoredFiles());
  REQUIRE_FALSE(defaults.usesUnsafeSorting());
}

TEST_CASE("The deployer proxy filters by activation status", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());
  TestDeployerListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);
  REQUIRE(proxy.rowCount() == 3);

  proxy.addFilter(DeployerListProxyModel::filter_active, false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 2);

  proxy.addFilter(DeployerListProxyModel::filter_inactive, false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 1);

  proxy.removeFilter(DeployerListProxyModel::filter_inactive, false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 2);

  proxy.clearFilter(false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 3);
}

TEST_CASE("The deployer proxy filters by conflicts", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());
  TestDeployerListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.setConflicts({ 1, 3 });
  proxy.addFilter(DeployerListProxyModel::filter_conflicts, false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 2);

  proxy.removeFilter(DeployerListProxyModel::filter_conflicts, false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 3);
}

TEST_CASE("The deployer proxy filters by tags", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());
  TestDeployerListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.addFilter(DeployerListProxyModel::filter_tags, false);
  proxy.addTagFilter("zzz", true, false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 1);
  REQUIRE(proxy.getTagFilters() == std::vector<std::pair<QString, bool>>{ { "zzz", true } });

  proxy.addTagFilter("zzz", false, false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 2);

  proxy.removeTagFilter("zzz", false);
  proxy.invalidate();
  REQUIRE(proxy.rowCount() == 3);
  REQUIRE(proxy.getTagFilters().empty());
}

TEST_CASE("The deployer proxy filters by filter string", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());
  TestDeployerListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.setFilterString("Beta");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("gamma");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("ID: 2");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("3");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("");
  REQUIRE(proxy.rowCount() == 3);
}

TEST_CASE("The deployer proxy matches source mod names", "[deployerlist]")
{
  DeployerInfo info;
  info.mod_names = { "Mod A", "Mod B" };
  info.loadorder = { { 7, true }, { -1, true } };
  info.ids_are_source_references = true;
  info.source_mod_names_ = { "Base Unpacker", "Sourceless" };
  DeployerListModel model;
  model.setDeployerInfo(info);
  TestDeployerListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.setFilterString("Unpacker");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("Mod");
  REQUIRE(proxy.rowCount() == 2);
  proxy.setFilterString("Sourceless");
  REQUIRE(proxy.rowCount() == 1);
}

TEST_CASE("The deployer proxy passes data through and keeps it read only", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());
  TestDeployerListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  REQUIRE(proxy.data(proxy.index(0, DeployerListModel::name_col)).toString() == "Alpha");
  REQUIRE_FALSE(proxy.setData(proxy.index(0, DeployerListModel::name_col), "Renamed", Qt::EditRole));
  REQUIRE(proxy.data(proxy.index(0, DeployerListModel::name_col)).toString() == "Alpha");
}

TEST_CASE("The deployer proxy tracks filter mode mutually exclusively", "[deployerlist]")
{
  DeployerListModel model;
  model.setDeployerInfo(threeModInfo());
  TestDeployerListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  REQUIRE(proxy.getFilterMode() == 0);
  proxy.addFilter(DeployerListProxyModel::filter_active, false);
  REQUIRE(proxy.getFilterMode() == DeployerListProxyModel::filter_active);
  proxy.addFilter(DeployerListProxyModel::filter_inactive, false);
  REQUIRE(proxy.getFilterMode() == DeployerListProxyModel::filter_inactive);
  proxy.clearFilter(false);
  REQUIRE(proxy.getFilterMode() == 0);
}
