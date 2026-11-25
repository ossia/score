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
  if(plug->recording(ctx))
  {
    auto stop = menu.addAction(QObject::tr("Stop recording"));
    QObject::connect(stop, &QAction::triggered, plug, [plug] { plug->stopSession(); });
    return;
  }

  auto sub = menu.addMenu(QObject::tr("Record"));
  sub->setToolTipsVisible(true);
  if(plug->session())
  {
    sub->addAction(QObject::tr("Recording in another document"))->setEnabled(false);
    return;
  }

  const auto addresses = addressesToRecord(explorer);
  if(addresses.empty() || !doc)
  {
    sub->addAction(QObject::tr("Select devices or addresses to record them"))
        ->setEnabled(false);
    return;
  }

  // Into the box of the document's main node view
  addAddressEntries(*sub, menu, *plug, doc->baseInterval(), addresses, {}, ctx);
}
}
