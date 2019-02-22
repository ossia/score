// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProjectSettingsFactory.hpp"

#include "ProjectSettingsModel.hpp"
#include "ProjectSettingsPresenter.hpp"
#include "ProjectSettingsView.hpp"

namespace score
{
ProjectSettingsFactory::~ProjectSettingsFactory() = default;

ProjectSettingsPresenter* ProjectSettingsFactory::makePresenter(
    ProjectSettingsModel& m, ProjectSettingsView& v, QObject* parent)
{
  auto p = makePresenter_impl(m, v, parent);
  v.setPresenter(p);

  return p;
}
}
