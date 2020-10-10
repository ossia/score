#include "SynthChainInspector.hpp"

#include <Media/Commands/InsertEffect.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#if defined(HAS_FAUST)
#include <Media/Commands/EditFaustEffect.hpp>
#include <Media/Effect/Faust/FaustEffectModel.hpp>
#endif

#if defined(HAS_LV2)
#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/LV2/LV2EffectModel.hpp>

#include <lilv/lilvmm.hpp>

#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#endif

#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTEffectModel.hpp>
#endif

#include <Process/ProcessList.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>

#include <Effect/EffectLayer.hpp>
namespace Media::SynthChain
{
InspectorWidget::InspectorWidget(
    const SynthChain::ProcessModel& object,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{object, parent}, m_dispatcher{doc.commandStack}, m_ctx{doc}
{
  setObjectName("EffectInspectorWidget");

  auto lay = new QVBoxLayout;
  m_list = new QListWidget;
  lay->addWidget(m_list);

  object.effects().added.connect<&InspectorWidget::on_effectAdded>(this);
  object.effects().removed.connect<&InspectorWidget::on_effectRemoved>(this);
  object.effects().orderChanged.connect<&InspectorWidget::on_orderChanged>(this);

  connect(
      m_list,
      &QListWidget::itemDoubleClicked,
      &object,
      [&](QListWidgetItem* item) {
        Process::setupExternalUI(
            object.effects().at(item->data(Qt::UserRole).value<Id<Process::ProcessModel>>()),
            doc,
            true);
      },
      Qt::QueuedConnection);

  m_list->setContextMenuPolicy(Qt::DefaultContextMenu);
  recreate();

  this->setLayout(lay);
}

void InspectorWidget::on_effectAdded(const Process::ProcessModel& p)
{
  recreate();
}

void InspectorWidget::on_effectRemoved(const Process::ProcessModel& p)
{
  recreate();
}

void InspectorWidget::on_orderChanged()
{
  recreate();
}

void InspectorWidget::addRequested(int pos) { }

int InspectorWidget::cur_pos()
{
  if (m_list->currentRow() >= 0)
    return m_list->currentRow();
  return process().effects().size();
}
struct ListWidgetItem : public QObject, public QListWidgetItem
{
public:
  using QListWidgetItem::QListWidgetItem;
};

void InspectorWidget::recreate()
{
  m_list->clear();

  for (const auto& fx : process().effects())
  {
    auto item = new ListWidgetItem(fx.prettyName(), m_list);

    con(fx.metadata(), &score::ModelMetadata::LabelChanged, item, [=](const auto& txt) {
      item->setText(txt);
    });
    item->setData(Qt::UserRole, QVariant::fromValue(fx.id()));
    m_list->addItem(item);
  }
}
}
