// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalTreePresenter.hpp"
#include "LocalTreeModel.hpp"
#include "LocalTreeView.hpp"
#include <QApplication>
#include <QStyle>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Command.hpp>
#include <score/command/SettingsCommand.hpp>

namespace Engine
{
namespace LocalTree
{
namespace Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::SettingsDelegatePresenter{m, v, parent}
{
  con(v, &View::localTreeChanged, this, [&](auto val) {
    if (val != m.getLocalTree())
    {
      m_disp.submitDeferredCommand<SetModelLocalTree>(this->model(this), val);
    }
  });

  con(m, &Model::LocalTreeChanged, &v, &View::setLocalTree);
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
