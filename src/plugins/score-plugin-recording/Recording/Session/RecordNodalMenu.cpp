#include "RecordNodalMenu.hpp"
#include "RecordMenus.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessContext.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <Recording/ApplicationPlugin.hpp>

#include <score/selection/SelectionStack.hpp>

#include <QMenu>
#include <QPointF>
#include <QPointer>

namespace Recording
{
void RecordNodalMenu::setupMenu(
    QMenu& menu, QObject& interval, QPointF pos, const Process::Context& ctx)
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
  // One recording at a time, in the whole application
  if(plug->session())
  {
    auto busy = menu.addAction(QObject::tr("Recording in another document"));
    busy->setEnabled(false);
    return;
  }

  auto sub = menu.addMenu(QObject::tr("Record"));
  sub->setToolTipsVisible(true);

  // The outputs of the processes selected, as the Record button does
  bool processes = false;
  for(const auto& obj : ctx.selectionStack.currentSelection())
    processes = processes || qobject_cast<const Process::ProcessModel*>(obj.data())
                || qobject_cast<const Process::Port*>(obj.data());
  {
    auto act = sub->addAction(
        processes ? QObject::tr("Record the selected processes")
                  : QObject::tr("Record the selected processes (none selected)"));
    act->setEnabled(processes);
    act->setToolTip(QObject::tr("Their audio and MIDI outputs, or the ports selected"));
    QObject::connect(
        act, &QAction::triggered, plug, [plug, &ctx, pos] { plug->recordSelection(ctx, pos); });
  }

  // What the explorer has selected, into this box
  const auto addresses = plug->selectedAddresses(ctx);
  if(addresses.empty())
  {
    auto act = sub->addAction(
        QObject::tr("Select devices or addresses in the explorer to record them"));
    act->setEnabled(false);
    return;
  }
  auto addr = sub->addMenu(QObject::tr("Record the selected addresses"));
  addr->setToolTipsVisible(true);
  addAddressEntries(*addr, menu, *plug, *box, addresses, pos, ctx);
}
}
