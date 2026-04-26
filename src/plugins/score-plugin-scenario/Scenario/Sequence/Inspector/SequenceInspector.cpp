// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceInspector.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Sequence/Commands/SetSequenceNamespace.hpp>

#include <Device/Node/NodeListMimeSerialization.hpp>

#include <Process/Dataflow/Port.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/MimeVisitor.hpp>
#include <score/tools/Bind.hpp>

#include <State/Address.hpp>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QVBoxLayout>

namespace Sequence
{

InspectorWidget::InspectorWidget(
    const SequenceModel& model, const score::DocumentContext& ctx, QWidget* parent)
    : InspectorWidgetDelegate_T{model, parent}
    , m_ctx{ctx}
    , m_dispatcher{ctx.dispatcher}
{
  setAcceptDrops(true);

  auto layout = new QVBoxLayout{this};
  layout->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(new QLabel{tr("Parameters (drop from Device Explorer)"), this});

  m_listWidget = new QListWidget{this};
  m_listWidget->setDragDropMode(QAbstractItemView::DropOnly);
  layout->addWidget(m_listWidget);

  rebuildList();

  con(model, &SequenceModel::namespaceChanged, this, &InspectorWidget::rebuildList);
  con(model, &SequenceModel::structureChanged, this, &InspectorWidget::rebuildList);
}

void InspectorWidget::rebuildList()
{
  m_listWidget->clear();

  const auto& seq = process();

  // Explicitly managed addresses
  for(const auto& addr : seq.parameterNamespace())
  {
    auto row = new QWidget{m_listWidget};
    auto rl = new QHBoxLayout{row};
    rl->setContentsMargins(2, 1, 2, 1);

    auto lbl = new QLabel{addr.toString(), row};
    lbl->setToolTip(tr("Drop to remove"));
    rl->addWidget(lbl, 1);

    auto rm = new QPushButton{tr("×"), row};
    rm->setFixedWidth(20);
    rm->setFlat(true);
    const State::AddressAccessor capturedAddr = addr;
    connect(rm, &QPushButton::clicked, this, [this, capturedAddr] {
      m_dispatcher.submit<Sequence::Command::RemoveSequenceParameter>(
          process(), capturedAddr);
    });
    rl->addWidget(rm);

    auto item = new QListWidgetItem{m_listWidget};
    item->setSizeHint(row->sizeHint());
    m_listWidget->addItem(item);
    m_listWidget->setItemWidget(item, row);
  }

  // Read-only: addresses from sibling processes in parent interval
  if(auto* itv = qobject_cast<Scenario::IntervalModel*>(seq.parent()))
  {
    bool hasSiblings = false;
    for(const auto& proc : itv->processes)
    {
      if(&proc == &seq)
        continue;
      for(const auto* outlet : proc.outlets())
      {
        const auto& addr = outlet->address();
        if(addr.address.device.isEmpty())
          continue;

        if(!hasSiblings)
        {
          auto sep = new QListWidgetItem{tr("— from sibling processes —"), m_listWidget};
          sep->setFlags(Qt::NoItemFlags);
          m_listWidget->addItem(sep);
          hasSiblings = true;
        }

        auto item = new QListWidgetItem{addr.toString(), m_listWidget};
        item->setFlags(Qt::NoItemFlags);
        m_listWidget->addItem(item);
      }
    }
  }
}

void InspectorWidget::dragEnterEvent(QDragEnterEvent* event)
{
  if(event->mimeData()->hasFormat(score::mime::nodelist()))
    event->acceptProposedAction();
}

void InspectorWidget::dropEvent(QDropEvent* event)
{
  if(!event->mimeData()->hasFormat(score::mime::nodelist()))
    return;

  Mime<Device::FreeNodeList>::Deserializer des{*event->mimeData()};
  const Device::FreeNodeList nl = des.deserialize();

  for(const auto& node : nl)
  {
    State::AddressAccessor addr{node.first};
    if(addr.address.device.isEmpty())
      continue;
    m_dispatcher.submit<Sequence::Command::AddSequenceParameter>(process(), addr);
  }

  event->acceptProposedAction();
}

}
