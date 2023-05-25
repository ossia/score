#include "ScenarioInspectorFactory.hpp"

#include <Scenario/Commands/Scenario/Properties.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QBoxLayout>
#include <QToolButton>

namespace Scenario
{

QWidget* InspectorWidgetDelegateFactory::makeProcess(
    const Process::ProcessModel& process, const score::DocumentContext& doc,
    QWidget* parent) const
{
  return wrap(process, doc, nullptr, parent);
}

bool InspectorWidgetDelegateFactory::matchesProcess(
    const Process::ProcessModel& process) const
{
  return dynamic_cast<const Scenario::ProcessModel*>(&process);
}

void InspectorWidgetDelegateFactory::addButtons(
    const Process::ProcessModel& proc, const score::DocumentContext& doc,
    QBoxLayout* layout, QWidget* parent) const
{
  auto& model = safe_cast<const Scenario::ProcessModel&>(proc);
  {
    auto sigWidg = new QToolButton{parent};

    sigWidg->setIcon(makeIcons(
        QStringLiteral(":/icons/exclusive_on.png"),
        QStringLiteral(":/icons/exclusive_hover.png"),
        QStringLiteral(":/icons/exclusive_off.png"),
        QStringLiteral(":/icons/exclusive_off.png")));
    sigWidg->setToolTip(QObject::tr("Exclusive"));
    sigWidg->setStatusTip(QObject::tr(
        "Make the scenario operate in exclusive mode: in this mode, if an interval is "
        "started in a component, all the other components will be disabled. It is "
        "mainly useful when one wants a sub-scenario where starting a sound or a video "
        "should stop "
        "all the others."));
    sigWidg->setCheckable(true);
    sigWidg->setAutoRaise(true);
    sigWidg->setChecked(model.exclusive());
    sigWidg->setIconSize(QSize{28, 28});

    QObject::connect(
        sigWidg, &QToolButton::toggled, (QObject*)&proc, [&doc, &model](bool b) {
          if(b != model.exclusive())
          {
            CommandDispatcher<> disp{doc.commandStack};
            disp.submit<Command::SetScenarioExclusive>(model, b);
          }
        });

    layout->addWidget(sigWidg);
  }
}

}
