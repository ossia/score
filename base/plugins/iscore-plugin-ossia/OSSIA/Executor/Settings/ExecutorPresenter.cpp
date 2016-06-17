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
    iscore::SettingsDelegatePresenter{m, v, parent}
{
    // Execution rate
    con(v, &View::rateChanged,
        this, [&] (auto rate) {
        if(rate != m.getRate())
        {
            m_disp.submitCommand<SetModelRate>(this->model(this), rate);
        }
    });

    con(m, &Model::RateChanged, &v, &View::setRate);
    v.setRate(m.getRate());

    // Clock used
    std::map<QString, ClockManagerFactory::ConcreteFactoryKey> clockMap;
    for(auto& fact : m.clockFactories())
    {
        clockMap.insert(
                    std::make_pair(
                        fact.prettyName(),
                        fact.concreteFactoryKey()));
    }
    v.populateClocks(clockMap);

    con(v, &View::clockChanged,
        this, [&] (auto val) {
        if(val != m.getClock())
        {
            m_disp.submitCommand<SetModelClock>(
                        this->model(this), val);
        }
    });

    con(m, &Model::ClockChanged,
        &v, &View::setClock);

    v.setClock(m.getClock());
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
