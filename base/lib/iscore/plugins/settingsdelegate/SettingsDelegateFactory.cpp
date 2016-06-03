#include "SettingsDelegateFactory.hpp"
#include "SettingsDelegatePresenter.hpp"
#include "SettingsDelegateView.hpp"
#include "SettingsDelegateModel.hpp"

namespace iscore
{
SettingsDelegateFactory::~SettingsDelegateFactory() = default;

SettingsDelegatePresenter*SettingsDelegateFactory::makePresenter(
        SettingsDelegateModel& m,
        SettingsDelegateView& v,
        QObject* parent)
{
    auto p = makePresenter_impl(m, v, parent);
    v.setPresenter(p);

    return p;
}
}
