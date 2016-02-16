#include "ProjectSettingsFactory.hpp"
#include "ProjectSettingsPresenter.hpp"
#include "ProjectSettingsModel.hpp"
#include "ProjectSettingsView.hpp"

namespace iscore
{
ProjectSettingsFactory::~ProjectSettingsFactory()
{

}

ProjectSettingsPresenter* ProjectSettingsFactory::makePresenter(
        ProjectSettingsModel& m,
        ProjectSettingsView& v,
        QObject* parent)
{
    auto p = makePresenter_impl(m, v, parent);
    v.setPresenter(p);

    return p;
}
}
