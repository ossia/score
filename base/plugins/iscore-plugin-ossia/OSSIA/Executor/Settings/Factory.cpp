#include "Factory.hpp"
#include "Model.hpp"
#include "View.hpp"
#include "Presenter.hpp"
namespace RecreateOnPlay
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
