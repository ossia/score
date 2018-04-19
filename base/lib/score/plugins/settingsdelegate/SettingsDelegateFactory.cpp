// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SettingsDelegateFactory.hpp"

#include "SettingsDelegateModel.hpp"
#include "SettingsDelegatePresenter.hpp"
#include "SettingsDelegateView.hpp"

namespace score
{
SettingsDelegateFactory::~SettingsDelegateFactory() = default;

GlobalSettingsPresenter* SettingsDelegateFactory::makePresenter(
    SettingsDelegateModel& m, GlobalSettingsView& v, QObject* parent)
{
  auto p = makePresenter_impl(m, v, parent);
  v.setPresenter(p);

  return p;
}
}
