#include "LocalTreeFactory.hpp"
#include "LocalTreeModel.hpp"
#include "LocalTreeView.hpp"
#include "LocalTreePresenter.hpp"
namespace Ossia
{
namespace LocalTree
{
namespace Settings
{

iscore::SettingsDelegateViewInterface* Factory::makeView()
{
    return new View;
}

iscore::SettingsDelegatePresenterInterface* Factory::makePresenter_impl(
        iscore::SettingsDelegateModelInterface& m,
        iscore::SettingsDelegateViewInterface& v,
        QObject* parent)
{
    return new Presenter{
        static_cast<Model&>(m),
        static_cast<View&>(v),
        parent};
}

iscore::SettingsDelegateModelInterface* Factory::makeModel()
{
    return new Model;
}


}
}
}
