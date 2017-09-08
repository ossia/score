// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExecutorPresenter.hpp"
#include "ExecutorModel.hpp"
#include "ExecutorView.hpp"
#include <QApplication>
#include <QStyle>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/Command.hpp>
#include <score/command/SettingsCommand.hpp>

namespace Engine
{
namespace Execution
{
namespace Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::SettingsDelegatePresenter{m, v, parent}
{
  // Execution rate
  con(v, &View::rateChanged, this, [&](auto rate) {
    if (rate != m.getRate())
    {
      m_disp.submitCommand<SetModelRate>(this->model(this), rate);
    }
  });

  con(m, &Model::RateChanged, &v, &View::setRate);
  v.setRate(m.getRate());

  // Clock used
  std::map<QString, ClockManagerFactory::ConcreteKey> clockMap;
  for (auto& fact : m.clockFactories())
  {
    clockMap.insert(
        std::make_pair(fact.prettyName(), fact.concreteKey()));
  }
  v.populateClocks(clockMap);

  con(v, &View::clockChanged, this, [&](auto val) {
    if (val != m.getClock())
    {
      m_disp.submitCommand<SetModelClock>(this->model(this), val);
    }
  });

  con(m, &Model::ClockChanged, &v, &View::setClock);
  v.setClock(m.getClock());

  con(v, &View::executionListeningChanged, this, [&](auto val) {
      if (val != m.getExecutionListening())
      {
          m_disp.submitCommand<SetModelExecutionListening>(this->model(this), val);
      }
  });

  con(m, &Model::ExecutionListeningChanged, &v, &View::setExecutionListening);
  v.setExecutionListening(m.getExecutionListening());
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
}
