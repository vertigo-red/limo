#include "../src/ui/colors.h"
#include "../src/ui/modlistmodel.h"
#include "../src/ui/modlistproxymodel.h"
#include <QBrush>
#include <QRegularExpression>
#include <QStringList>
#include <catch2/catch_test_macros.hpp>


namespace
{
Mod makeMod(int id, const std::string& name, const std::string& version, uintmax_t size,
            const std::time_t& install_time = 0,
            const std::time_t& remote_update_time = 0,
            const std::time_t& suppress_update_time = 0)
{
  return Mod(id, name, version, install_time, {}, "", remote_update_time, size,
             suppress_update_time, -1, -1, ImportModInfo::RemoteType::local);
}

ModInfo makeInfo(Mod mod,
                 int group,
                 bool active,
                 const std::vector<std::string>& deployers = {},
                 const std::vector<int>& deployer_ids = {},
                 const std::vector<bool>& statuses = {},
                 const std::vector<std::string>& manual_tags = {},
                 const std::vector<std::string>& auto_tags = {})
{
  return ModInfo(mod, deployers, deployer_ids, statuses, group, active, manual_tags, auto_tags);
}

std::vector<ModInfo> sampleMods()
{
  std::vector<ModInfo> mods;
  mods.push_back(makeInfo(makeMod(1, "Alpha Mod", "1.0", 100), -1, false,
                          { "LoadOrder" }, { 10 }, { true }, { "combat" }, {}));
  mods.push_back(makeInfo(makeMod(2, "Beta Mod", "2.0", 200), -1, false,
                          {}, {}, { false }, { "stealth" }, {}));
  mods.push_back(makeInfo(makeMod(3, "Gamma Mod", "3.0", 300, 100, 200, 50), 7, true,
                          {}, {}, { true }, { "combat" }, { "new" }));
  mods.push_back(makeInfo(makeMod(12, "Delta Mod", "4.0", 400), -1, false,
                          {}, {}, { true }, {}, {}));
  mods.push_back(makeInfo(makeMod(5, "Epsilon Mod", "5.0", 500), -1, false,
                          {}, {}, {}, {}, {}));
  return mods;
}
} // namespace

TEST_CASE("An empty mod list model has eight columns and no rows", "[modlist]")
{
  ModListModel model(nullptr);
  REQUIRE(model.columnCount() == 8);
  REQUIRE(model.rowCount() == 0);
  REQUIRE_FALSE(model.data(QModelIndex()).isValid());
}

TEST_CASE("The mod list model exposes the column headers", "[modlist]")
{
  ModListModel model(nullptr);
  REQUIRE(model.headerData(ModListModel::action_col, Qt::Horizontal).toString() == "Action");
  REQUIRE(model.headerData(ModListModel::name_col, Qt::Horizontal).toString() == "Name");
  REQUIRE(model.headerData(ModListModel::version_col, Qt::Horizontal).toString() == "Version");
  REQUIRE(model.headerData(ModListModel::id_col, Qt::Horizontal).toString() == "ID");
  REQUIRE(model.headerData(ModListModel::time_col, Qt::Horizontal).toString() == "Installation Time");
  REQUIRE(model.headerData(ModListModel::size_col, Qt::Horizontal).toString() == "Size");
  REQUIRE(model.headerData(ModListModel::deployers_col, Qt::Horizontal).toString() == "Deployers");
  REQUIRE(model.headerData(ModListModel::tags_col, Qt::Horizontal).toString() == "Tags");
  REQUIRE_FALSE(model.headerData(8, Qt::Horizontal).isValid());
  REQUIRE(model.headerData(0, Qt::Vertical).toString() == "1");
  REQUIRE(model.headerData(ModListModel::deployers_col, Qt::Horizontal, Qt::TextAlignmentRole)
          == QVariant(Qt::AlignLeft));
}

TEST_CASE("The mod list model exposes display data for a mod", "[modlist]")
{
  ModListModel model(nullptr);
  std::vector<ModInfo> mods;
  mods.push_back(makeInfo(makeMod(1, "Mod One", "1.0", 1572864, 100, 200, 50), -1, false,
                          { "LoadOrder", "BG3" }, { 10, 20 }, { true, false },
                          { "zzz", "aaa" }, { "mid" }));
  model.setModInfo(mods);

  REQUIRE(model.rowCount() == 1);
  REQUIRE(model.data(model.index(0, ModListModel::name_col)).toString() == "Mod One");
  REQUIRE(model.data(model.index(0, ModListModel::version_col)).toString() == "1.0");
  REQUIRE(model.data(model.index(0, ModListModel::id_col)).toString() == "1");
  REQUIRE(model.data(model.index(0, ModListModel::size_col)).toString() == "1.5 MiB");
  REQUIRE(model.data(model.index(0, ModListModel::deployers_col)).toString() == "LoadOrder, BG3");
  REQUIRE(model.data(model.index(0, ModListModel::tags_col)).toString() == "[Has Update], aaa, mid, zzz");
  const auto time_string = model.data(model.index(0, ModListModel::time_col)).toString();
  REQUIRE(QRegularExpression(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$)").match(time_string).hasMatch());
}

TEST_CASE("The mod list model reports updates and paints them green", "[modlist]")
{
  ModListModel model(nullptr);
  std::vector<ModInfo> mods;
  mods.push_back(makeInfo(makeMod(1, "Mod One", "1.0", 100, 100, 200, 50), -1, false));
  model.setModInfo(mods);

  REQUIRE(model.data(model.index(0, 0), ModListModel::has_update_role).toBool());
  REQUIRE(model.data(model.index(0, ModListModel::name_col), Qt::ForegroundRole).value<QBrush>().color()
          == colors::GREEN);

  ModListModel stale_model(nullptr);
  stale_model.setModInfo({ makeInfo(makeMod(1, "Mod One", "1.0", 100), -1, false) });
  REQUIRE_FALSE(stale_model.data(stale_model.index(0, 0), ModListModel::has_update_role).toBool());
  REQUIRE_FALSE(
    stale_model.data(stale_model.index(0, ModListModel::name_col), Qt::ForegroundRole).isValid());
}

TEST_CASE("The mod list model formats sizes into human readable strings", "[modlist]")
{
  ModListModel model(nullptr);
  std::vector<ModInfo> mods;
  mods.push_back(makeInfo(makeMod(1, "A", "1.0", 500), -1, false));
  mods.push_back(makeInfo(makeMod(2, "B", "1.0", 2048), -1, false));
  mods.push_back(makeInfo(makeMod(3, "C", "1.0", 1572864), -1, false));
  mods.push_back(makeInfo(makeMod(4, "D", "1.0", 1610612736), -1, false));
  model.setModInfo(mods);

  REQUIRE(model.data(model.index(0, ModListModel::size_col)).toString() == "500 B");
  REQUIRE(model.data(model.index(1, ModListModel::size_col)).toString() == "2 KiB");
  REQUIRE(model.data(model.index(2, ModListModel::size_col)).toString() == "1.5 MiB");
  REQUIRE(model.data(model.index(3, ModListModel::size_col)).toString() == "1.5 GiB");
}

TEST_CASE("The mod list model exposes custom roles", "[modlist]")
{
  ModListModel model(nullptr);
  std::vector<ModInfo> mods;
  mods.push_back(makeInfo(makeMod(1, "Mod One", "1.5", 100), -1, false,
                          { "LoadOrder" }, { 10 }, { true }, { "aaa" }, { "auto" }));
  model.setModInfo(mods);

  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_id_role).toInt() == 1);
  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_name_role).toString() == "Mod One");
  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_version_role).toString() == "1.5");
  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_group_role).toInt() == -1);
  REQUIRE(model.data(model.index(0, 0), ModListModel::deployer_ids_role)
            .value<std::vector<int>>() == std::vector<int>{ 10 });
  REQUIRE(model.data(model.index(0, 0), ModListModel::statuses_role)
            .value<std::vector<bool>>() == std::vector<bool>{ true });
  REQUIRE(model.data(model.index(0, 0), ModListModel::manual_tags_role).toStringList()
          == QStringList({ "aaa" }));
  REQUIRE(model.data(model.index(0, 0), ModListModel::auto_tags_role).toStringList()
== QStringList({ "auto" }));
  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_size_role).value<qulonglong>() == 100);
  REQUIRE(model.data(model.index(0, 0), ModListModel::local_source_role).toString().isEmpty());
  REQUIRE(model.data(ModListModel::mod_id_role, 0, 0).toInt() == 1);
}

TEST_CASE("Grouped mods only expose the active member", "[modlist]")
{
  ModListModel model(nullptr);
  std::vector<ModInfo> mods;
  mods.push_back(makeInfo(makeMod(1, "Active Mod", "2.0", 1000), 5, true));
  mods.push_back(makeInfo(makeMod(2, "Hidden Mod", "1.0", 2000), 5, false));
  mods.push_back(makeInfo(makeMod(3, "Solo Mod", "1.0", 3000), -1, false));
  model.setModInfo(mods);

REQUIRE(model.rowCount() == 2);
  REQUIRE(model.data(model.index(0, 0), ModListModel::mod_group_role).toInt() == 5);
  REQUIRE(model.data(model.index(0, ModListModel::version_col), ModListModel::version_list_role)
            .toStringList() == QStringList({ "2.0", "1.0" }));
  REQUIRE(model.data(model.index(0, ModListModel::version_col), ModListModel::active_index_role).toInt()
          == 0);
  REQUIRE(model.getGroupMap().size() == 2);
  REQUIRE(model.getGroupMap().at(1) == 5);
  REQUIRE(model.getGroupMap().at(2) == 5);
}

TEST_CASE("Name and version columns are editable when the model is editable", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo({ makeInfo(makeMod(1, "Mod One", "1.0", 100), -1, false) });

  CHECK((model.flags(model.index(0, ModListModel::name_col)) & Qt::ItemIsEditable) != 0);
  CHECK((model.flags(model.index(0, ModListModel::version_col)) & Qt::ItemIsEditable) != 0);
  CHECK((model.flags(model.index(0, ModListModel::id_col)) & Qt::ItemIsEditable) == 0);
  CHECK(model.flags(QModelIndex()) == Qt::NoItemFlags);

  model.setIsEditable(false);
  CHECK_FALSE(model.isEditable());
  CHECK((model.flags(model.index(0, ModListModel::name_col)) & Qt::ItemIsEditable) == 0);
}

namespace
{
} // namespace

TEST_CASE("The proxy model filters by group membership", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);
  REQUIRE(proxy.rowCount() == 5);

  proxy.setFilterMode(ModListProxyModel::filter_groups, true);
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterMode(ModListProxyModel::filter_no_groups, true);
  REQUIRE(proxy.rowCount() == 4);
  proxy.clearFilter();
  REQUIRE(proxy.rowCount() == 5);
  proxy.setFilterMode(ModListProxyModel::filter_no_groups, true);
  REQUIRE(proxy.rowCount() == 4);
  proxy.setFilterMode(ModListProxyModel::filter_no_groups, false);
  REQUIRE(proxy.rowCount() == 5);
}

TEST_CASE("The proxy model filters by deployer activation status", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.addFilter(ModListProxyModel::filter_active);
  REQUIRE(proxy.rowCount() == 3);
  proxy.removeFilter(ModListProxyModel::filter_active);
  proxy.addFilter(ModListProxyModel::filter_inactive);
  REQUIRE(proxy.rowCount() == 2);
}

TEST_CASE("The proxy model filters by update availability", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.addFilter(ModListProxyModel::filter_updates);
  REQUIRE(proxy.rowCount() == 1);
  proxy.clearFilter();
  proxy.addFilter(ModListProxyModel::filter_no_updates);
  REQUIRE(proxy.rowCount() == 4);
}

TEST_CASE("The proxy model filters by tags", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.addTagFilter("combat", true, false);
  REQUIRE(proxy.getTagFilters() == std::vector<std::pair<QString, bool>>{ { "combat", true } });
  REQUIRE(proxy.getFilterMode() == 0);

proxy.addFilter(ModListProxyModel::filter_tags);
  REQUIRE(proxy.rowCount() == 2);

  proxy.addTagFilter("combat", false, true);
  REQUIRE(proxy.rowCount() == 3);
  REQUIRE(proxy.getTagFilters() == std::vector<std::pair<QString, bool>>{ { "combat", false } });

  proxy.removeTagFilter("combat", true);
  REQUIRE(proxy.getTagFilters().empty());
  REQUIRE(proxy.rowCount() == 5);

  proxy.removeFilter(ModListProxyModel::filter_tags);
  REQUIRE(proxy.rowCount() == 5);
}

TEST_CASE("The proxy model filters by filter string", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.setFilterString("Gamma");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("gamma");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("ID: 3");
  REQUIRE(proxy.rowCount() == 1);
  proxy.setFilterString("ID: 1");
  REQUIRE(proxy.rowCount() == 2);
  proxy.setFilterString("2");
  REQUIRE(proxy.rowCount() == 2);
  proxy.setFilterString("");
  REQUIRE(proxy.rowCount() == 5);
}

TEST_CASE("Filter modes are mutually exclusive", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.addFilter(ModListProxyModel::filter_groups);
  REQUIRE(proxy.getFilterMode() == ModListProxyModel::filter_groups);
  proxy.addFilter(ModListProxyModel::filter_no_groups);
  REQUIRE(proxy.getFilterMode() == ModListProxyModel::filter_no_groups);
  proxy.addFilter(ModListProxyModel::filter_active);
  REQUIRE(proxy.getFilterMode() == ModListProxyModel::filter_no_groups + ModListProxyModel::filter_active);
  proxy.removeFilter(ModListProxyModel::filter_active);
  proxy.clearFilter();
  REQUIRE(proxy.getFilterMode() == 0);
}

TEST_CASE("The proxy model sorts numerically by size", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);

  proxy.sort(ModListModel::size_col, Qt::AscendingOrder);
  REQUIRE(proxy.rowCount() == 5);
  REQUIRE(proxy.data(proxy.index(0, 0), ModListModel::mod_id_role).toInt() == 1);
  REQUIRE(proxy.data(proxy.index(4, 0), ModListModel::mod_id_role).toInt() == 5);

  proxy.sort(ModListModel::size_col, Qt::DescendingOrder);
  REQUIRE(proxy.data(proxy.index(0, 0), ModListModel::mod_id_role).toInt() == 5);
  REQUIRE(proxy.data(proxy.index(4, 0), ModListModel::mod_id_role).toInt() == 1);

  proxy.sort(ModListModel::name_col, Qt::AscendingOrder);
  REQUIRE(proxy.data(proxy.index(0, ModListModel::name_col)).toString() == "Alpha Mod");
  REQUIRE(proxy.data(proxy.index(4, ModListModel::name_col)).toString() == "Gamma Mod");
}

TEST_CASE("The proxy model passes data through and keeps it read only", "[modlist]")
{
  ModListModel model(nullptr);
  model.setModInfo(sampleMods());
  ModListProxyModel proxy(nullptr, nullptr);
  proxy.setSourceModel(&model);
  proxy.sort(ModListModel::size_col, Qt::AscendingOrder);

REQUIRE(proxy.data(proxy.index(0, ModListModel::name_col)).toString() == "Alpha Mod");
  REQUIRE_FALSE(proxy.setData(proxy.index(0, ModListModel::name_col), "Renamed", Qt::EditRole));
  REQUIRE(proxy.data(proxy.index(0, ModListModel::name_col)).toString() == "Alpha Mod");
}

