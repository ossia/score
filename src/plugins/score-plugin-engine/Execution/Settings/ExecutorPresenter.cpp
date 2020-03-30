// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExecutorPresenter.hpp"

#include "ExecutorModel.hpp"
#include "ExecutorView.hpp"

#include <score/command/Command.hpp>
#include <score/command/Dispatchers/ICommandDispatcher.hpp>
#include <score/command/SettingsCommand.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QStyle>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Execution::Settings::Model)
W_OBJECT_IMPL(Execution::Settings::View)
namespace Execution
{
namespace Settings
{
Presenter::Presenter(Model& m, View& v, QObject* parent)
    : score::GlobalSettingsPresenter{m, v, parent}
{
  SETTINGS_PRESENTER(Scheduling);
  SETTINGS_PRESENTER(Ordering);
  SETTINGS_PRESENTER(Merging);
  SETTINGS_PRESENTER(Commit);
  SETTINGS_PRESENTER(Tick);
  SETTINGS_PRESENTER(Parallel);
  SETTINGS_PRESENTER(Rate);
  SETTINGS_PRESENTER(Logging);
  SETTINGS_PRESENTER(Bench);
  SETTINGS_PRESENTER(ExecutionListening);
  SETTINGS_PRESENTER(ScoreOrder);
  SETTINGS_PRESENTER(ValueCompilation);
  SETTINGS_PRESENTER(TransportValueCompilation);

  // Clock used
  std::map<QString, ClockFactory::ConcreteKey> clockMap;
  for (auto& fact : m.clockFactories())
  {
    clockMap.insert(std::make_pair(fact.prettyName(), fact.concreteKey()));
  }
  v.populateClocks(clockMap);

  con(v, &View::ClockChanged, this, [&](auto val) {
    if (val.impl().data != m.getClock().impl().data)
    {
      m_disp.submit<SetModelClock>(this->model(this), val);
    }
  });

  con(m, &Model::ClockChanged, &v, &View::setClock);
  v.setClock(m.getClock());

  con(v, &View::ExecutionListeningChanged, this, [&](auto val) {
    if (val != m.getExecutionListening())
    {
      m_disp.submit<SetModelExecutionListening>(this->model(this), val);
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
  return makeIcons(QStringLiteral(":/icons/settings_play_on.png")
                   , QStringLiteral(":/icons/settings_play_off.png")
                   , QStringLiteral(":/icons/settings_play_off.png"));
}
}
}
