#include "ExecutorPresenter.hpp"
#include "ExecutorModel.hpp"
#include "ExecutorView.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <QApplication>
#include <QStyle>
#include <iscore/command/SettingsCommand.hpp>

namespace RecreateOnPlay
{
namespace Settings
{
Presenter::Presenter(
        Model& m,
        View& v,
        QObject *parent):
    iscore::SettingsDelegatePresenterInterface{m, v, parent}
{
    con(v, &View::rateChanged,
        this, [&] (auto rate) {
        if(rate != m.getRate())
        {
            m_disp.submitCommand<SetModelRate>(this->model(this), rate);
        }
    });

    con(m, &Model::RateChanged, &v, &View::setRate);
    v.setRate(m.getRate());
}

QString Presenter::settingsName()
{
    return tr("Execution");
}

QIcon Presenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
}


}
}
