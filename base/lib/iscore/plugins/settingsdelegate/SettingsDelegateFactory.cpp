#include "SettingsDelegateFactoryInterface.hpp"
#include "SettingsDelegatePresenterInterface.hpp"
#include "SettingsDelegateViewInterface.hpp"
#include "SettingsDelegateModelInterface.hpp"

namespace iscore
{
SettingsDelegateFactory::~SettingsDelegateFactory() = default;

SettingsDelegatePresenterInterface*SettingsDelegateFactory::makePresenter(
        SettingsDelegateModelInterface& m,
        SettingsDelegateViewInterface& v,
        QObject* parent)
{
    auto p = makePresenter_impl(m, v, parent);
    v.setPresenter(p);

    return p;
}
}
