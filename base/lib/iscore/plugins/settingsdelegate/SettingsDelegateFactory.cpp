#include "SettingsDelegateFactoryInterface.hpp"
#include "SettingsDelegatePresenterInterface.hpp"
#include "SettingsDelegateViewInterface.hpp"
#include "SettingsDelegateModelInterface.hpp"

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
