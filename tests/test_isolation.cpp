/*!
 * \file test_isolation.cpp
 * \brief Gives every test case an isolated copy of the shared data fixtures.
 *
 * Limo's tests operate on a shared `tests/data` tree (staging, app, deployer
 * target directories, source fixtures). When tests are discovered by
 * `catch_discover_tests` each case runs in its own process but still reads and
 * writes the same on-disk `tests/data`, so a test that mutates a fixture can
 * leak state into the next test and make results order dependent.
 *
 * This listener copies the pristine `tests/data` tree into a throwaway sandbox
 * before every test case and repoints `DATA_DIR` at it, then removes the
 * sandbox afterwards. The checked-in `tests/data` is never written to, making
 * the suite order independent.
 */

#include "test_utils.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/reporters/catch_event_listener.hpp>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>

namespace sfs = std::filesystem;

namespace
{

class PerTestDataIsolation : public Catch::EventListenerBase
{
public:
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const&) override
  {
    std::lock_guard<std::mutex> lock(mutex_);
    int run = counter_.fetch_add(1);
    sandbox_ = BASE_PATH / ("data_test_" + std::to_string(run));
    if(sfs::exists(sandbox_))
      sfs::remove_all(sandbox_);
    sfs::copy(BASE_PATH / "data",
              sandbox_,
              sfs::copy_options::recursive | sfs::copy_options::create_directories);
    DATA_DIR = sandbox_;
  }

  void testCaseEnded(Catch::TestCaseStats const&) override
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!sandbox_.empty() && sfs::exists(sandbox_))
      sfs::remove_all(sandbox_);
    sandbox_.clear();
    DATA_DIR = BASE_PATH / "data";
  }

private:
  std::mutex mutex_;
  sfs::path sandbox_;
  static std::atomic<int> counter_;
};

std::atomic<int> PerTestDataIsolation::counter_{ 0 };

CATCH_REGISTER_LISTENER(PerTestDataIsolation)

}