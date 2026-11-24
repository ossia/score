#include "RecordExplorerMenu.hpp"

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
  const QPointer<Scenario::IntervalModel> box = &doc->baseInterval();
  const std::pair<AddressPlacement, QString> kinds[]{
      {AddressPlacement::Automations, QObject::tr("Record as automations")},
      {AddressPlacement::Messages, QObject::tr("Record as messages")}};
  for(const auto& [placement, text] : kinds)
  {
    auto act = sub->addAction(text);
    act->setToolTip(QObject::tr(
        "Into the main box of the document, from now: audio inputs as sounds, "
        "cameras and other video inputs as videos, the other addresses as %1")
                        .arg(placement == AddressPlacement::Automations
                                 ? QObject::tr("automations")
                                 : QObject::tr("messages")));
    QObject::connect(
        act, &QAction::triggered, plug, [plug, box, addresses, placement = placement] {
      if(box)
        plug->recordAddressesInto(*box, addresses, placement);
    });
  }
}
}
