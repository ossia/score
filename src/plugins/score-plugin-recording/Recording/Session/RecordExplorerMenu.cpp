#include "RecordExplorerMenu.hpp"
#include "RecordMenus.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Recording/ApplicationPlugin.hpp>
#include <Recording/Record/RecordTools.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <QMenu>
#include <QPointer>

namespace Recording
{
void RecordExplorerMenu::setupMenu(
    QMenu& menu, Explorer::DeviceExplorerModel& explorer, const score::DocumentContext& ctx)
{
  auto* plug = ctx.app.findGuiApplicationPlugin<Recording::ApplicationPlugin>();
  auto* doc = score::IDocument::try_get<Scenario::ScenarioDocumentModel>(ctx.document);
  if(!plug)
    return;
  if(!doc)
    return;
  // Into the box of the document's main node view
  RecordMenuTarget target;
  target.box = &doc->baseInterval();
  addRecordEntries(menu, *plug, ctx, target);
}
}
