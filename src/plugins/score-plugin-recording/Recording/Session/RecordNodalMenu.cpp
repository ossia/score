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

  RecordMenuTarget target;
  target.box = box;
  target.nodePosition = pos;
  addRecordEntries(menu, *plug, ctx, target);
}
}
