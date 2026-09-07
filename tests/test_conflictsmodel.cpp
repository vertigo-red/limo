#include "../src/ui/colors.h"
#include "../src/ui/conflictsmodel.h"
#include <catch2/catch_test_macros.hpp>
#include <QColor>


namespace
{
ConflictInfo makeConflict(const std::string& file, const std::vector<int>& ids)
{
  ConflictInfo info;
  info.file = file;
  info.mod_ids = ids;
  for(const int id : ids)
    info.mod_names.push_back("Mod " + std::to_string(id));
  return info;
}
} // namespace

TEST_CASE("An empty conflicts model has no rows", "[conflicts]")
{
  ConflictsModel model;
  REQUIRE(model.rowCount() == 0);
  REQUIRE(model.columnCount() == 3);
  REQUIRE_FALSE(model.data(model.index(0, ConflictsModel::file_col)).isValid());
}

TEST_CASE("Horizontal headers expose the column names", "[conflicts]")
{
  ConflictsModel model;
  REQUIRE(model.headerData(ConflictsModel::file_col, Qt::Horizontal).toString() == "File");
  REQUIRE(model.headerData(ConflictsModel::winner_col, Qt::Horizontal).toString() == "Winner");
  REQUIRE(model.headerData(ConflictsModel::order_col, Qt::Horizontal).toString() == "Overwrite order");
  REQUIRE_FALSE(model.headerData(3, Qt::Horizontal).isValid());
  REQUIRE_FALSE(model.headerData(0, Qt::Vertical).isValid());
  REQUIRE(model.headerData(ConflictsModel::order_col,
                           Qt::Horizontal,
                           Qt::TextAlignmentRole) == QVariant(Qt::AlignLeft));
}

TEST_CASE("Conflicts expose file, winner and overwrite order", "[conflicts]")
{
  ConflictsModel model;
  model.setConflicts({ makeConflict("mod.esp", { 1, 2 }), makeConflict("texture.dds", { 3 }) }, 2);

  REQUIRE(model.rowCount() == 2);
  REQUIRE(model.data(model.index(0, ConflictsModel::file_col)).toString() == "mod.esp");
  REQUIRE(model.data(model.index(0, ConflictsModel::winner_col)).toString() == "Mod 2 [2]");
  REQUIRE(
    model.data(model.index(0, ConflictsModel::order_col)).toString() == "Mod 1 [1] ==> Mod 2 [2]");
  REQUIRE(model.data(model.index(1, ConflictsModel::winner_col)).toString() == "Mod 3 [3]");
}

TEST_CASE("The winning mod foreground color depends on the base mod", "[conflicts]")
{
  ConflictsModel model;
  model.setConflicts({ makeConflict("mod.esp", { 1, 2 }) }, 2);
  REQUIRE(model.data(model.index(0, ConflictsModel::file_col), Qt::ForegroundRole).value<QColor>()
          == colors::GREEN);

  ConflictsModel losing_model;
  losing_model.setConflicts({ makeConflict("mod.esp", { 1, 2 }) }, 3);
  REQUIRE(losing_model.data(model.index(0, ConflictsModel::file_col), Qt::ForegroundRole)
              .value<QColor>()
          == colors::RED);
}

TEST_CASE("Out-of-range columns and rows yield no data", "[conflicts]")
{
  ConflictsModel model;
  model.setConflicts({ makeConflict("mod.esp", { 1 }) }, 1);
  REQUIRE_FALSE(model.data(model.index(0, 3)).isValid());
  REQUIRE_FALSE(model.data(model.index(1, 0)).isValid());
  REQUIRE_FALSE(model.data(QModelIndex()).isValid());
}