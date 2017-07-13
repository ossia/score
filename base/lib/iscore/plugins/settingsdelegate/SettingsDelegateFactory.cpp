// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SettingsDelegateFactory.hpp"
#include "SettingsDelegateModel.hpp"
#include "SettingsDelegatePresenter.hpp"
#include "SettingsDelegateView.hpp"

namespace iscore
{
SettingsDelegateFactory::~SettingsDelegateFactory() = default;

SettingsDelegatePresenter* SettingsDelegateFactory::makePresenter(
    SettingsDelegateModel& m, SettingsDelegateView& v, QObject* parent)
{
  auto p = makePresenter_impl(m, v, parent);
  v.setPresenter(p);

  return p;
}
}
