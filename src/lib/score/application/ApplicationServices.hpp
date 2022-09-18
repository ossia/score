#pragma once
#include <score/tools/ThreadPool.hpp>
#include <score/tools/FileWatch.hpp>
#include <score/tools/RecursiveWatch.hpp>

namespace score
{
// These service have some dependency order - filewatch has to be deleted
// before threadpool for instance.
// Actual usage should be through the individual classes, not this.
// This way if e.g. the taskpool is never needed it will not be initialized.
struct SCORE_LIB_BASE_EXPORT ApplicationServices
{
  std::optional<score::ThreadPool> threadpool;
  std::optional<score::TaskPool> taskpool;
  std::optional<score::FileWatch> filewatch;
};

SCORE_LIB_BASE_EXPORT
ApplicationServices& AppServices() noexcept;
}
