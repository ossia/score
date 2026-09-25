#include "ReferencesDialog.hpp"

#include <Process/Dataflow/AddressAccessorEditWidget.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/State/RebindReference.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <LocalTree/LocalTreeDocumentPlugin.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace LocalTree
{
namespace
{
QString describe(QObject& referrer)
{
  if(auto s = qobject_cast<Scenario::StateModel*>(&referrer))
    return QObject::tr("State %1").arg(s->metadata().getName());
  if(auto p = qobject_cast<Process::Port*>(&referrer))
  {
    if(auto proc = Process::parentProcess(p))
      return QObject::tr("Port %1 of %2").arg(p->name(), proc->metadata().getName());
    return QObject::tr("Port %1").arg(p->name());
  }
  if(auto e = qobject_cast<Scenario::EventModel*>(&referrer))
    return QObject::tr("Condition of %1").arg(e->metadata().getName());
  if(auto t = qobject_cast<Scenario::TimeSyncModel*>(&referrer))
    return QObject::tr("Trigger of %1").arg(t->metadata().getName());
  return referrer.objectName();
}

//! The object to select to show a referrer; for a port, its process
const IdentifiedObjectAbstract* selectable(QObject& referrer)
{
  if(auto p = qobject_cast<Process::Port*>(&referrer))
    return Process::parentProcess(p);
  return dynamic_cast<IdentifiedObjectAbstract*>(&referrer);
}
}

ReferencesDialog::ReferencesDialog(const score::GUIApplicationContext& ctx, QWidget* parent)
    : QDialog{parent}
{
  setWindowTitle(tr("Broken references"));
  setModal(false);
  resize(640, 360);
  auto m_widget = this;
  auto lay = new QVBoxLayout{m_widget};
  m_summary = new QLabel{m_widget};
  m_tree = new QTreeWidget{m_widget};
  m_tree->setHeaderLabels({tr("Address"), tr("Held by"), tr("Problem")});
  m_tree->setRootIsDecorated(false);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);

  auto buttons = new QWidget{m_widget};
  auto blay = new score::MarginLess<QHBoxLayout>{buttons};
  m_locate = new QPushButton{QObject::tr("Locate"), buttons};
  m_rebind = new QPushButton{QObject::tr("Rebind..."), buttons};
  blay->addWidget(m_locate);
  blay->addWidget(m_rebind);
  blay->addStretch();

  lay->addWidget(m_summary);
  lay->addWidget(m_tree);
  lay->addWidget(buttons);

  connect(m_locate, &QPushButton::clicked, this, [this] { locate(); });
  connect(m_rebind, &QPushButton::clicked, this, [this] { rebind(); });
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this] { locate(); });
  refresh();
}

ReferencesDialog::~ReferencesDialog() = default;

void ReferencesDialog::setDocument(const score::DocumentContext* doc)
{
  QObject::disconnect(m_connection);
  m_doc = doc;
  if(m_doc)
    if(auto plug = m_doc->findPlugin<DocumentPlugin>())
      m_connection = connect(
          &plug->references(), &ReferenceIndex::changed, this, [this] { refresh(); });
  refresh();
}

void ReferencesDialog::refresh()
{
  m_tree->clear();
  auto plug = m_doc ? m_doc->findPlugin<DocumentPlugin>() : nullptr;
  if(!plug)
  {
    m_summary->setText(QObject::tr("No document"));
    return;
  }

  const auto broken = plug->references().broken();
  for(auto& [address, referrer, status] : broken)
  {
    auto item = new QTreeWidgetItem{
        {address.toString(), describe(*referrer), LocalTree::describe(status)}};
    item->setData(0, Qt::UserRole, QVariant::fromValue(referrer));
    item->setData(0, Qt::UserRole + 1, address.toString());
    m_tree->addTopLevelItem(item);
  }
  m_summary->setText(
      broken.empty()
          ? tr("Every reference of the document resolves.")
          : tr("%n reference(s) point at something the document does not have.", "",
               int(broken.size())));
  m_locate->setEnabled(!broken.empty());
  m_rebind->setEnabled(!broken.empty());
}

void ReferencesDialog::locate()
{
  auto item = m_tree->currentItem();
  if(!item || !m_doc)
    return;
  auto referrer = item->data(0, Qt::UserRole).value<QObject*>();
  if(!referrer)
    return;
  if(auto obj = selectable(*referrer))
    score::SelectionDispatcher{m_doc->selectionStack}.select(*obj);
}

void ReferencesDialog::rebind()
{
  auto item = m_tree->currentItem();
  if(!item || !m_doc)
    return;
  // The dialog below runs an event loop, during which the referrer may be deleted
  QPointer<QObject> referrer = item->data(0, Qt::UserRole).value<QObject*>();
  auto from = ::State::Address::fromString(item->data(0, Qt::UserRole + 1).toString());
  if(!referrer || !from)
    return;

  QDialog dialog{this};
  dialog.setWindowTitle(QObject::tr("Rebind %1").arg(from->toString()));
  auto lay = new QVBoxLayout{&dialog};
  auto edit = new Process::AddressAccessorEditWidget{*m_doc, &dialog};
  edit->setAddress(::State::AddressAccessor{*from});
  auto buttons = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  lay->addWidget(new QLabel{QObject::tr("Address to point at instead:"), &dialog});
  lay->addWidget(edit);
  lay->addWidget(buttons);
  if(dialog.exec() != QDialog::Accepted)
    return;

  const auto to = edit->address().address.address;
  if(!referrer || !m_doc || !to.isSet() || to == *from)
    return;
  Scenario::Command::rebindReference(*referrer, *from, to, *m_doc);
}
}
