#include "RecordNodalMenu.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Recording/ApplicationPlugin.hpp>

#include <score/selection/SelectionStack.hpp>

#include <QMenu>
#include <QPointer>

namespace Recording
{
void RecordNodalMenu::setupMenu(
    QMenu& menu, QObject& interval, const Process::Context& ctx)
{
  auto* box = qobject_cast<Scenario::IntervalModel*>(&interval);
  auto* plug = ctx.app.findGuiApplicationPlugin<Recording::ApplicationPlugin>();
  if(!box || !plug)
    return;

  if(plug->recording(ctx))
  {
    auto stop = menu.addAction(QObject::tr("Stop recording"));
    QObject::connect(stop, &QAction::triggered, plug, [plug] { plug->stopSession(); });
    return;
  }

  auto sub = menu.addMenu(QObject::tr("Record"));

  // The outputs of the processes selected, as the Record button does
  bool processes = false;
  for(const auto& obj : ctx.selectionStack.currentSelection())
    processes = processes || qobject_cast<const Process::ProcessModel*>(obj.data())
                || qobject_cast<const Process::Port*>(obj.data());
  {
    auto act = sub->addAction(QObject::tr("Record the selected processes"));
    act->setEnabled(processes);
    act->setToolTip(QObject::tr("Their audio and MIDI outputs, or the ports selected"));
    QObject::connect(
        act, &QAction::triggered, plug, [plug, &ctx] { plug->recordSelection(ctx); });
  }

  // What the explorer has selected, into this box
  const auto addresses = plug->selectedAddresses(ctx);
  const QPointer<Scenario::IntervalModel> target = box;
  const std::pair<AddressPlacement, QString> kinds[]{
      {AddressPlacement::Automations,
       QObject::tr("Record the selected addresses as automations")},
      {AddressPlacement::Messages, QObject::tr("Record the selected addresses as messages")}};
  for(const auto& [placement, text] : kinds)
  {
    auto act = sub->addAction(text);
    act->setEnabled(!addresses.empty());
    act->setToolTip(QObject::tr(
        "The addresses selected in the device explorer, into this box from now: "
        "audio inputs as sounds, cameras as videos"));
    QObject::connect(
        act, &QAction::triggered, plug, [plug, target, addresses, placement = placement] {
      if(target)
        plug->recordAddressesInto(*target, addresses, placement);
    });
  }
}
}
