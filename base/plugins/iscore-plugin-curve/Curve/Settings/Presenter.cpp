#include "Presenter.hpp"
#include "Model.hpp"
#include "View.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <QApplication>
#include <QStyle>
#include <iscore/command/SettingsCommand.hpp>

namespace Curve
{
namespace Settings
{
Presenter::Presenter(
        Model& m,
        View& v,
        QObject *parent):
    iscore::SettingsDelegatePresenterInterface{m, v, parent}
{
    con(v, &View::simplificationRatioChanged,
        this, [&] (auto simplificationRatio) {
        if(simplificationRatio != m.getSimplificationRatio())
        {
            m_disp.submitCommand<SetSimplificationRatio>(this->model(this), simplificationRatio);
        }
    });

    con(m, &Model::simplificationRatioChanged, &v, &View::setSimplificationRatio);
    v.setSimplificationRatio(m.getSimplificationRatio());
}

QString Presenter::settingsName()
{
    return tr("Recording");
}

QIcon Presenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_DriveCDIcon);
}


}
}
