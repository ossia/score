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

    con(v, &View::simplifyChanged,
        this, [&] (auto simplify) {
        if(simplify != m.getSimplify())
        {
            m_disp.submitCommand<SetSimplify>(this->model(this), simplify);
        }
    });

    con(m, &Model::simplifyChanged, &v, &View::setSimplify);
    v.setSimplify(m.getSimplify());
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
