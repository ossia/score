#include "ExecutorFactory.hpp"
#include "ExecutorModel.hpp"
#include "ExecutorView.hpp"
#include "ExecutorPresenter.hpp"
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
