#include "LocalTreePresenter.hpp"
#include "LocalTreeModel.hpp"
#include "LocalTreeView.hpp"
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/command/Dispatchers/ICommandDispatcher.hpp>
#include <QApplication>
#include <QStyle>
#include <iscore/command/SettingsCommand.hpp>

namespace Ossia
{
namespace LocalTree
{
namespace Settings
{
Presenter::Presenter(
        Model& m,
        View& v,
        QObject *parent):
    iscore::SettingsDelegatePresenterInterface{m, v, parent}
{
    con(v, &View::localTreeChanged,
        this, [&] (auto val) {
        if(val != m.getLocalTree())
        {
            m_disp.submitCommand<SetLocalTree>(this->model(this), val);
        }
    });

    con(m, &Model::localTreeChanged, &v, &View::setLocalTree);
    v.setLocalTree(m.getLocalTree());
}

QString Presenter::settingsName()
{
    return tr("Local tree");
}

QIcon Presenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
}


}
}
}
