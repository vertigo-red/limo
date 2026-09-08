#include "../src/core/backuptarget.h"
#include "../src/ui/backuplistmodel.h"
#include <QStringList>
#include <catch2/catch_test_macros.hpp>


namespace
{
std::vector<BackupTarget> sampleTargets()
{
  return {
    BackupTarget("C:/data/saves", "Save Game", { "Save 1", "Save 2" }, { 0 }),
    BackupTarget("C:/data/config", "Settings", { "Default" }, { 0 }),
  };
}
} // namespace

TEST_CASE("An empty backup list model only contains the add-target row", "[backuplist]")
{
  BackupListModel model;
  REQUIRE(model.columnCount() == 4);
  REQUIRE(model.rowCount() == 1);
  REQUIRE_FALSE(model.data(model.index(0, BackupListModel::target_col)).isValid());
}

TEST_CASE("The backup list model exposes the column headers", "[backuplist]")
{
  BackupListModel model;
  REQUIRE(model.headerData(BackupListModel::action_col, Qt::Horizontal).toString() == "Action");
  REQUIRE(model.headerData(BackupListModel::target_col, Qt::Horizontal).toString() == "Target");
  REQUIRE(model.headerData(BackupListModel::backup_col, Qt::Horizontal).toString() == "Backup");
  REQUIRE(model.headerData(BackupListModel::path_col, Qt::Horizontal).toString() == "Path");
  REQUIRE_FALSE(model.headerData(4, Qt::Horizontal).isValid());
  REQUIRE(model.headerData(BackupListModel::path_col, Qt::Horizontal, Qt::TextAlignmentRole)
          == QVariant(Qt::AlignLeft));
}

TEST_CASE("The backup list model exposes display data for targets", "[backuplist]")
{
  BackupListModel model;
  model.setBackupTargets(sampleTargets());

  REQUIRE(model.rowCount() == 3);
  REQUIRE(model.data(model.index(0, BackupListModel::target_col)).toString() == "Save Game");
  REQUIRE(model.data(model.index(0, BackupListModel::backup_col)).toString() == "Save 1");
  REQUIRE(model.data(model.index(0, BackupListModel::path_col)).toString() == "C:/data/saves");
  REQUIRE(model.data(model.index(1, BackupListModel::target_col)).toString() == "Settings");
  REQUIRE_FALSE(model.data(model.index(2, BackupListModel::target_col)).isValid());
}

TEST_CASE("The backup list model exposes custom roles", "[backuplist]")
{
  BackupListModel model;
  model.setBackupTargets(sampleTargets());

  REQUIRE(model.data(model.index(0, 0), BackupListModel::backup_list_role).toStringList()
          == QStringList({ "Save 1", "Save 2" }));
  REQUIRE(model.data(model.index(0, 0), BackupListModel::active_index_role).toInt() == 0);
  REQUIRE(model.data(model.index(0, 0), BackupListModel::num_backups_role).value<size_t>() == 2);
  REQUIRE(model.data(model.index(0, 0), BackupListModel::num_targets_role).value<size_t>() == 2);
  REQUIRE(model.data(model.index(0, 0), BackupListModel::target_name_role).toString() == "Save Game");
  REQUIRE(model.data(model.index(0, 0), BackupListModel::backup_name_role).toString() == "Save 1");
  REQUIRE(model.data(model.index(0, 0), BackupListModel::target_path_role).toString()
          == "C:/data/saves");
}

TEST_CASE("Target and backup columns are editable when the model is editable", "[backuplist]")
{
  BackupListModel model;
  model.setBackupTargets(sampleTargets());

  CHECK((model.flags(model.index(0, BackupListModel::target_col)) & Qt::ItemIsEditable) != 0);
  CHECK((model.flags(model.index(0, BackupListModel::backup_col)) & Qt::ItemIsEditable) != 0);
  CHECK((model.flags(model.index(0, BackupListModel::action_col)) & Qt::ItemIsEditable) == 0);
  CHECK((model.flags(model.index(0, BackupListModel::path_col)) & Qt::ItemIsEditable) == 0);
  CHECK((model.flags(model.index(2, BackupListModel::target_col)) & Qt::ItemIsEditable) == 0);
  CHECK(model.flags(QModelIndex()) == Qt::NoItemFlags);

  model.setIsEditable(false);
  CHECK_FALSE(model.isEditable());
  CHECK((model.flags(model.index(0, BackupListModel::target_col)) & Qt::ItemIsEditable) == 0);
}