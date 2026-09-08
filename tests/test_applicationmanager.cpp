#include "../src/ui/applicationmanager.h"
#include "../src/core/deployerfactory.h"
#include "../src/core/editapplicationinfo.h"
#include "../src/core/editdeployerinfo.h"
#include "../src/core/installer.h"
#include "../src/core/log.h"
#include <QByteArray>
#include <QCoreApplication>
#include <QSettings>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <filesystem>


/*!
 * The ApplicationManager stores its state using QSettings. Only one instance may
 * exist at a time. Each test gets its own temporary XDG_CONFIG_HOME so the
 * settings file never touches the real user configuration, plus a staging
 * directory. The Installer::log callback installed by the manager is reset when
 * the environment goes away so later tests are not left with a dangling lambda.
 *
 * QSignalSpy lives in Qt5::Test, which the nix build root does not link, so the
 * tests record signals with plain QObject connections instead.
 */
namespace
{
std::filesystem::path makeUniqueDir(const std::string& base)
{
  static std::atomic<unsigned> counter{ 0 };
  return std::filesystem::temp_directory_path() /
         ("lmm_app_mgr_test_" + base + "_" + std::to_string(++counter));
}

class AppMgrEnv
{
public:
  explicit AppMgrEnv(const std::string& base)
  {
    work_dir_ = makeUniqueDir(base);
    staging_dir_ = work_dir_ / "staging";
    std::filesystem::create_directories(staging_dir_);
    old_config_home_ = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", (work_dir_ / "config").string().c_str());
    QCoreApplication::setApplicationName("limo_test");
  }

  ~AppMgrEnv()
  {
    Installer::log = [](Log::LogLevel, const std::string&) {};
    if(old_config_home_.isNull())
      qunsetenv("XDG_CONFIG_HOME");
    else
      qputenv("XDG_CONFIG_HOME", old_config_home_);
    std::error_code ec;
    std::filesystem::remove_all(work_dir_, ec);
  }

  std::filesystem::path staging_dir_;

private:
  std::filesystem::path work_dir_;
  QByteArray old_config_home_;
};

EditApplicationInfo makeAppInfo(const std::filesystem::path& staging_dir,
                                const std::string& name = "test app")
{
  EditApplicationInfo info;
  info.name = name;
  info.staging_dir = staging_dir.string();
  info.command = "run-game";
  info.icon_path = "";
  info.app_version = "1.0";
  return info;
}

class SignalRecorder : public QObject
{
public:
  SignalRecorder(ApplicationManager* mgr)
  {
    QObject::connect(mgr, &ApplicationManager::sendApplicationNames, this,
                     [this](QStringList names, QStringList, bool is_new)
                     { app_names_.push_back(names); app_names_new_.append(is_new); });
    QObject::connect(mgr, &ApplicationManager::sendDeployerNames, this,
                     [this](QStringList names, bool) { depl_names_.push_back(names); });
    QObject::connect(mgr, &ApplicationManager::sendError, this,
                     [this](QString, QString) { errors_++; });
    QObject::connect(mgr, &ApplicationManager::scrollLists, this,
                     [this]() { scrolls_++; });
    QObject::connect(mgr, &ApplicationManager::updateProgress, this,
                     [this](float p) { progress_.push_back(p); });
    QObject::connect(mgr, &ApplicationManager::completedOperations, this,
                     [this](QString) { completed_++; });
  }

  std::vector<QStringList> app_names_;
  QList<bool> app_names_new_;
  std::vector<QStringList> depl_names_;
  int errors_ = 0;
  int scrolls_ = 0;
  std::vector<float> progress_;
  int completed_ = 0;
};
}  // namespace


TEST_CASE("The application manager refuses a second instance", "[appmgr]")
{
  AppMgrEnv env("second");
  ApplicationManager mgr;
  REQUIRE(mgr.getNumApplications() == 0);

  bool threw = false;
  try
  {
    ApplicationManager second_instance(nullptr);
  }
  catch(const std::runtime_error&)
  {
    threw = true;
  }
  REQUIRE(threw);
}

TEST_CASE("The application manager starts empty for a fresh profile", "[appmgr]")
{
  AppMgrEnv env("fresh");
  ApplicationManager mgr;
  mgr.init();
  REQUIRE(mgr.getNumApplications() == 0);
  REQUIRE(mgr.getNumProfiles(0) == 0);
  REQUIRE(mgr.toString().empty());
}

TEST_CASE("The application manager adds an application and persists it", "[appmgr]")
{
  AppMgrEnv env("add");
  ApplicationManager mgr;
  SignalRecorder rec(&mgr);

  mgr.addApplication(makeAppInfo(env.staging_dir_, "Game"));
  REQUIRE(mgr.getNumApplications() == 1);
  REQUIRE(mgr.getNumProfiles(0) == 1);
  REQUIRE(mgr.getNumProfiles(99) == 0);
  REQUIRE(mgr.toString().find("Game") != std::string::npos);
  REQUIRE(mgr.toString().find("Default") != std::string::npos);

  mgr.getApplicationNames(false);
  REQUIRE(rec.app_names_.size() == 1);
  REQUIRE(rec.app_names_.back().contains("Game"));
  REQUIRE(rec.app_names_new_.back() == false);
}

TEST_CASE("A missing staging directory prevents adding an application", "[appmgr]")
{
  AppMgrEnv env("missing");
  ApplicationManager mgr;
  SignalRecorder rec(&mgr);

  mgr.addApplication(makeAppInfo(env.staging_dir_.parent_path() / "does_not_exist"));
  REQUIRE(mgr.getNumApplications() == 0);
  REQUIRE(rec.errors_ == 1);
  REQUIRE(rec.completed_ >= 1);
}

TEST_CASE("The application manager restores its state from settings on init", "[appmgr]")
{
  AppMgrEnv env("restore");
  {
    ApplicationManager mgr;
    mgr.addApplication(makeAppInfo(env.staging_dir_, "Game"));
    REQUIRE(mgr.getNumApplications() == 1);
  }
  {
    ApplicationManager restored;
    SignalRecorder rec(&restored);
    restored.init();
    REQUIRE(restored.getNumApplications() == 1);
    REQUIRE(restored.toString().find("Game") != std::string::npos);

    restored.getApplicationNames(false);
    REQUIRE(rec.app_names_.size() == 1);
    REQUIRE(rec.app_names_.back().contains("Game"));
  }
}

TEST_CASE("Invalid application indices emit errors and are ignored", "[appmgr]")
{
  AppMgrEnv env("badapp");
  ApplicationManager mgr;
  mgr.addApplication(makeAppInfo(env.staging_dir_));

  SignalRecorder rec(&mgr);
  mgr.removeApplication(7, false);
  mgr.addProfile(7, { "extra", "1.0", -1 });
  mgr.addModToGroup(7, 1, 0);
  mgr.getDeployerNames(7, false);
  REQUIRE(rec.errors_ == 3);
  REQUIRE(mgr.getNumApplications() == 1);
  REQUIRE(mgr.getNumProfiles(0) == 1);
}

TEST_CASE("Deployer index guards emit errors for an app without deployers", "[appmgr]")
{
  AppMgrEnv env("nodepl");
  ApplicationManager mgr;
  mgr.addApplication(makeAppInfo(env.staging_dir_));

  SignalRecorder rec(&mgr);
  mgr.getDeployerNames(0, false);
  REQUIRE(rec.depl_names_.size() == 1);
  REQUIRE(rec.depl_names_.back().isEmpty());

  mgr.setModStatus(0, 0, 1, true);
  mgr.changeLoadorder(0, 0, 0, 1);
  mgr.editDeployer({}, 0, 0);
  mgr.addModToIgnoreList(0, 0, 1);
  REQUIRE(rec.errors_ == 4);
}

TEST_CASE("The application manager adds applications with deployers", "[appmgr]")
{
  AppMgrEnv env("withdepl");
  ApplicationManager mgr;
  SignalRecorder rec(&mgr);

  EditDeployerInfo first;
  first.type = DeployerFactory::SIMPLEDEPLOYER;
  first.name = "Main";
  first.target_dir = (env.staging_dir_ / "deploy").string();
  first.deploy_mode = Deployer::copy;

  EditApplicationInfo info = makeAppInfo(env.staging_dir_, "Game");
  info.deployers.push_back(first);
  mgr.addApplication(info);
  REQUIRE(mgr.getNumApplications() == 1);

  mgr.getDeployerNames(0, false);
  REQUIRE(rec.depl_names_.size() == 1);
  REQUIRE(rec.depl_names_.back() == QStringList{ "Main" });

  EditDeployerInfo second;
  second.type = DeployerFactory::CASEMATCHINGDEPLOYER;
  second.name = "Extra";
  second.target_dir = (env.staging_dir_ / "deploy2").string();
  second.deploy_mode = Deployer::copy;
  mgr.addDeployer(0, second);

  mgr.getDeployerNames(0, false);
  REQUIRE(rec.depl_names_.size() == 2);
  REQUIRE(rec.depl_names_.back() == QStringList{ "Main", "Extra" });

  mgr.getDeployerInfo(0, 0);
  mgr.getDeployerInfo(0, 1);
  mgr.getDeployerInfo(0, 99);
}

TEST_CASE("Slots ignore invalid app ids on an empty manager", "[appmgr]")
{
  AppMgrEnv env("guards");
  ApplicationManager mgr;

  mgr.deployMods(0);
  mgr.unDeployMods(0);
  mgr.installMod(0, ImportModInfo{});
  mgr.uninstallMods(0, { 1 }, "");
  mgr.setProfile(0, 0);
  mgr.removeProfile(0, 0);
  mgr.addTool(0, Tool{});
  mgr.changeModName(0, 1, "new");
  mgr.addBackupTarget(0, "path", "name", "default", "");
  mgr.overwriteBackup(0, 0, 0, 1);
  mgr.getExternalChanges(0, 0, true);
  mgr.checkForModUpdates(0);
  mgr.exportAppConfiguration(0, { 0 }, {});
  mgr.applyModAction(0, 0, 0, 1);

  REQUIRE(mgr.getNumApplications() == 0);
}

TEST_CASE("The application manager forwards helper signals", "[appmgr]")
{
  AppMgrEnv env("signals");
  ApplicationManager mgr;
  SignalRecorder rec(&mgr);

  mgr.onScrollLists();
  REQUIRE(rec.scrolls_ == 1);

  mgr.sendUpdateProgress(0.42f);
  REQUIRE(rec.progress_.size() == 1);
  REQUIRE(rec.progress_.back() == Catch::Approx(0.42f));
}

TEST_CASE("Removing an application updates the stored settings", "[appmgr]")
{
  AppMgrEnv env("remove");
  ApplicationManager mgr;
  mgr.addApplication(makeAppInfo(env.staging_dir_));
  REQUIRE(mgr.getNumApplications() == 1);

  mgr.removeApplication(0, false);
  REQUIRE(mgr.getNumApplications() == 0);
  REQUIRE(mgr.toString().empty());

  QSettings settings(QCoreApplication::applicationName());
  const int num_stored = settings.beginReadArray("staging_directories");
  REQUIRE(num_stored == 0);
  settings.endArray();
}