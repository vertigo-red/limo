#include "../src/core/changelogentry.h"
#include "../src/core/manualtag.h"
#include "../src/core/parseerror.h"
#include "../src/core/progressnode.h"
#include "../src/core/tag.h"
#include "../src/core/versionchangelog.h"
#include "../src/core/wildcardmatching.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <json/json.h>
#include <vector>


TEST_CASE("Wildcard expressions split into substrings", "[wildcard]")
{
  REQUIRE(splitString("").empty());
  REQUIRE(splitString("abc").size() == 1);
  REQUIRE(splitString("abc")[0] == "abc");
  REQUIRE(splitString("a*b*c") == std::vector<std::string>{ "a", "b", "c" });
  REQUIRE(splitString("a**b") == std::vector<std::string>{ "a", "b" });
  REQUIRE(splitString("*ab*") == std::vector<std::string>{ "ab" });
  REQUIRE(splitString("***") == std::vector<std::string>{});
}

TEST_CASE("Wildcard expressions match strings", "[wildcard]")
{
  REQUIRE(wildcardMatch("foo", "foo"));
  REQUIRE(wildcardMatch("foo", "*"));
  REQUIRE(wildcardMatch("foo", "***"));
  REQUIRE(wildcardMatch("foo", "f*"));
  REQUIRE(wildcardMatch("foo", "*o"));
  REQUIRE(wildcardMatch("foo", "f*o"));
  REQUIRE(wildcardMatch("foobar", "foo*bar"));
  REQUIRE(wildcardMatch("anything here", "*"));

  REQUIRE_FALSE(wildcardMatch("", ""));
  REQUIRE_FALSE(wildcardMatch("foo", ""));
  REQUIRE_FALSE(wildcardMatch("foo", "bar"));
  REQUIRE_FALSE(wildcardMatch("foo", "f*bar"));
  REQUIRE_FALSE(wildcardMatch("abc", "*d*"));
}

TEST_CASE("Wildcard expressions enforce anchors when not wildcarded", "[wildcard]")
{
  REQUIRE_FALSE(wildcardMatch("foobar", "bar"));
  REQUIRE_FALSE(wildcardMatch("foobar", "foo"));
  REQUIRE(wildcardMatch("foobar", "*bar"));
  REQUIRE(wildcardMatch("foobar", "foo*"));
  REQUIRE(wildcardMatch("foobar", "*oob*"));
  REQUIRE_FALSE(wildcardMatch("foo", "*ooX*"));
  REQUIRE_FALSE(wildcardMatch("foo", "a*"));
  REQUIRE_FALSE(wildcardMatch("foo", "*z"));
}

TEST_CASE("Changelog entries deserialize from JSON", "[changelog]")
{
  Json::Value json;
  json["type"] = ChangelogEntry::fix;
  json["short_description"] = "Fixed a bug";
  json["long_description"] = "A longer explanation";
  json["issue"] = 42;
  json["pull_request"] = 7;

  const ChangelogEntry entry(json);
  REQUIRE(entry.getType() == ChangelogEntry::fix);
  REQUIRE(entry.getShortDescription() == "Fixed a bug");
  REQUIRE(entry.getLongDescription() == "A longer explanation");
  REQUIRE(entry.getIssue() == 42);
  REQUIRE(entry.getPullRequest() == 7);
}

TEST_CASE("Changelog entries default missing issue and PR to -1", "[changelog]")
{
  Json::Value json;
  json["type"] = ChangelogEntry::new_feature;
  json["short_description"] = "New feature";

  const ChangelogEntry entry(json);
  REQUIRE(entry.getIssue() == -1);
  REQUIRE(entry.getPullRequest() == -1);
  REQUIRE(entry.getLongDescription().empty());
}

TEST_CASE("Changelog entries compare by type", "[changelog]")
{
  Json::Value fix_json;
  fix_json["type"] = ChangelogEntry::fix;
  Json::Value feature_json;
  feature_json["type"] = ChangelogEntry::new_feature;
  Json::Value change_json;
  change_json["type"] = ChangelogEntry::change;

  const ChangelogEntry fix(fix_json);
  const ChangelogEntry feature(feature_json);
  const ChangelogEntry change(change_json);

  REQUIRE(feature < fix);
  REQUIRE(feature < change);
  REQUIRE(change < fix);
  REQUIRE_FALSE(fix < change);
}

TEST_CASE("Version changelogs deserialize and sort entries", "[changelog]")
{
  Json::Value json;
  json["version"] = "1.2.0";
  json["date"] = Json::Int64(1700000000);
  json["title"] = "Big release";
  Json::Value change_a;
  change_a["type"] = ChangelogEntry::fix;
  change_a["short_description"] = "fix";
  Json::Value change_b;
  change_b["type"] = ChangelogEntry::new_feature;
  change_b["short_description"] = "feature";
  json["changes"][0] = change_a;
  json["changes"][1] = change_b;

  const VersionChangelog log(json);
  REQUIRE(log.getVersion() == "1.2.0");
  REQUIRE(log.getDate() == 1700000000);
  REQUIRE(log.getTitle() == "Big release");
  REQUIRE(log.getChanges().size() == 2);
  REQUIRE(log.getChanges()[0].getType() == ChangelogEntry::new_feature);
  REQUIRE(log.getChanges()[1].getType() == ChangelogEntry::fix);
}

TEST_CASE("Version changelogs compare by date", "[changelog]")
{
  Json::Value older_json;
  older_json["version"] = "1.0.0";
  older_json["date"] = Json::Int64(1000);
  Json::Value newer_json;
  newer_json["version"] = "2.0.0";
  newer_json["date"] = Json::Int64(2000);

  const VersionChangelog older(older_json);
  const VersionChangelog newer(newer_json);
  REQUIRE(older < newer);
  REQUIRE_FALSE(newer < older);
}

TEST_CASE("Progress nodes track leaf progress and cap at one", "[progress]")
{
  ProgressNode root([](float) {});
  root.setUpdateStepSize(0.0f);
  root.setTotalSteps(10);
  root.addChildren({ 1.0f });

  ProgressNode& leaf = root.child(0);
  leaf.setTotalSteps(4);
  leaf.advance(2);
  REQUIRE(leaf.getProgress() == Catch::Approx(0.5f));
  REQUIRE(root.getProgress() == Catch::Approx(0.5f));

  leaf.advance(2);
  REQUIRE(leaf.getProgress() == Catch::Approx(1.0f));

  leaf.setTotalSteps(3);
  leaf.advance(100);
  REQUIRE(leaf.getProgress() == Catch::Approx(1.0f));
  REQUIRE(leaf.totalSteps() == 3);
}

TEST_CASE("Progress node weights are normalized and made absolute", "[progress]")
{
  ProgressNode root([](float) {});
  root.setUpdateStepSize(0.0f);
  root.addChildren({ 1.0f, -2.0f, 0.0f });
  ProgressNode& zero = root.child(0);
  ProgressNode& one = root.child(1);
  ProgressNode& two = root.child(2);
  zero.setTotalSteps(1);
  one.setTotalSteps(1);
  two.setTotalSteps(1);

  zero.advance();  // weight 1/3
  REQUIRE(root.getProgress() == Catch::Approx(1.0f / 3.0f));
  one.advance();  // weight 2/3 -> total done
  REQUIRE(root.getProgress() == Catch::Approx(1.0f));
}

TEST_CASE("Progress node only leaves may be advanced", "[progress]")
{
  ProgressNode root([](float) {});
  root.setUpdateStepSize(0.0f);
  root.addChildren({ 1.0f });
  REQUIRE_THROWS_AS(root.advance(), std::runtime_error);
  REQUIRE_THROWS_AS(root.setTotalSteps(5), std::runtime_error);
}

TEST_CASE("Progress node ids and children are accessible", "[progress]")
{
  ProgressNode root([](float) {});
  root.addChildren({ 0.5f, 0.5f });
  REQUIRE(root.id() == 0);
  REQUIRE(root.child(0).id() == 0);
  REQUIRE(root.child(1).id() == 1);
}

TEST_CASE("Progress callback fires on completion regardless of step size", "[progress]")
{
  std::vector<float> calls;
  ProgressNode root([&calls](float p) { calls.push_back(p); });
  root.setUpdateStepSize(1.0f);
  root.setTotalSteps(2);

  REQUIRE(calls.size() == 1);  // initial call from setProgressCallback (progress 0)
  REQUIRE(calls.back() == Catch::Approx(0.0f));

  root.advance();  // 0.5 < 1.0 step size -> no callback
  REQUIRE(calls.size() == 1);

  root.advance();  // reaches 1.0 -> reported
  REQUIRE(calls.size() == 2);
  REQUIRE(calls.back() == Catch::Approx(1.0f));
}

TEST_CASE("Progress callback fires above the configured step size", "[progress]")
{
  std::vector<float> calls;
  ProgressNode root([&calls](float p) { calls.push_back(p); });
  root.setUpdateStepSize(0.5f);
  root.setTotalSteps(4);

  root.advance();  // 0.25 -> 0.25 - 0 = 0.25 <= 0.5, not reported
  REQUIRE(calls.size() == 1);

  root.advance();  // 0.5 -> delta 0.5 not > 0.5, not reported
  REQUIRE(calls.size() == 1);

  root.advance();  // 0.75 -> delta 0.75 > 0.5, reported
  REQUIRE(calls.size() == 2);
  REQUIRE(calls.back() == Catch::Approx(0.75f));
}

TEST_CASE("Manual tags add and remove mods", "[tag]")
{
  ManualTag tag(std::string("combat"));
  REQUIRE(tag.getName() == "combat");
  REQUIRE_FALSE(tag.hasMod(1));
  REQUIRE(tag.getNumMods() == 0);

  tag.addMod(1);
  tag.addMod(2);
  tag.addMod(1);  // duplicate is ignored
  REQUIRE(tag.hasMod(1));
  REQUIRE(tag.hasMod(2));
  REQUIRE(tag.getNumMods() == 2);
  REQUIRE(tag.getMods() == std::vector<int>{ 1, 2 });

  tag.removeMod(1);
  REQUIRE_FALSE(tag.hasMod(1));
  REQUIRE(tag.getNumMods() == 1);
}

TEST_CASE("Manual tags serialize and deserialize from JSON", "[tag]")
{
  ManualTag tag(std::string("combat"));
  tag.addMod(3);
  tag.addMod(4);

  const Json::Value json = tag.toJson();
  REQUIRE(json["name"].asString() == "combat");
  REQUIRE(json["mod_ids"].size() == 2);

  const ManualTag restored(json);
  REQUIRE(restored.getName() == "combat");
  REQUIRE(restored.hasMod(3));
  REQUIRE(restored.hasMod(4));
}

TEST_CASE("Manual tags compare by name and support setMods", "[tag]")
{
  ManualTag a(std::string("alpha"));
  ManualTag b(std::string("beta"));
  REQUIRE(a == "alpha");
  REQUIRE(a == a);
  REQUIRE_FALSE(a == b);

  a.setMods({ 10, 20 });
  REQUIRE(a.getMods() == std::vector<int>{ 10, 20 });
}

TEST_CASE("Manual tag deserialization rejects a missing name", "[tag]")
{
  Json::Value json;
  json["mod_ids"][0] = 1;
  REQUIRE_THROWS_AS(ManualTag(json), ParseError);
}
