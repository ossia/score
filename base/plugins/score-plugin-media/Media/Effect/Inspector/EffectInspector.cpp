#include "EffectInspector.hpp"

#include <Media/Commands/InsertEffect.hpp>
#include <Media/Commands/EditFaustEffect.hpp>
#include <score/document/DocumentContext.hpp>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QInputDialog>
#include <QFileDialog>

#if defined(HAS_FAUST)
#include <Media/Effect/Faust/FaustEffectModel.hpp>
#endif

#if defined(LILV_SHARED)
#include <lilv/lilvmm.hpp>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <Media/Effect/LV2/LV2EffectModel.hpp>
#include <Media/ApplicationPlugin.hpp>
#endif

#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTEffectModel.hpp>
#endif

#include <Scenario/DialogWidget/AddProcessDialog.hpp>
namespace Media
{
namespace Effect
{

InspectorWidget::InspectorWidget(
    const Effect::ProcessModel &object,
    const score::DocumentContext &doc,
    QWidget *parent):
  InspectorWidgetDelegate_T {object, parent},
  m_dispatcher{doc.commandStack}
, m_ctx{doc}
{
  setObjectName("EffectInspectorWidget");

  auto lay = new QVBoxLayout;
  m_list = new QListWidget;
  lay->addWidget(m_list);
  con(process(), &Effect::ProcessModel::effectsChanged,
      this, &InspectorWidget::recreate);


  connect(m_list, &QListWidget::itemDoubleClicked,
          this, [=] (QListWidgetItem* item) {
    // Make a text dialog to edit faust program.
    auto id = item->data(Qt::UserRole).value<Id<Process::EffectModel>>();
    auto proc = &process().effects().at(id);
    proc->hideUI();
    proc->showUI();
  }, Qt::QueuedConnection);

  m_list->setContextMenuPolicy(Qt::DefaultContextMenu);
  recreate();

  // Add an effect
  {
    auto add = new QPushButton{tr("Add (Score)")};
    connect(add, &QPushButton::pressed,
            this, [&] { add_score(cur_pos()); });
    lay->addWidget(add);
  }
  {
#if defined(HAS_FAUST)
    m_add = new QPushButton{tr("Add (Faust)")};
    connect(m_add, &QPushButton::pressed,
            this, [&] { add_faust(cur_pos()); });

    lay->addWidget(m_add);
#endif
  }

  {
#if defined(LILV_SHARED)
    auto add = new QPushButton{tr("Add (LV2)")};
    connect(add, &QPushButton::pressed,
            this, [&] { add_lv2(cur_pos()); });

    lay->addWidget(add);
#endif
  }

#if defined(HAS_VST2)
  {
    auto add = new QPushButton{tr("Add (VST 2)")};
    connect(add, &QPushButton::pressed,
            this, [&] { add_vst2(cur_pos()); });

    lay->addWidget(add);
  }
#endif

  // Remove an effect
  // Effects changed
  // Effect list
  // Double-click : open editor window.

  this->setLayout(lay);
}

void InspectorWidget::add_score(std::size_t pos)
{
  auto& base_fxs = m_ctx.app.interfaces<Process::EffectFactoryList>();
  auto dialog = new Scenario::AddProcessDialog<Process::EffectFactoryList>(base_fxs, this);


  dialog->on_okPressed = [&] (const auto& proc)  {
    m_dispatcher.submitCommand(
          new Commands::InsertEffect{process(), proc, "", pos});
  };
  dialog->launchWindow();
  dialog->deleteLater();
}

void InspectorWidget::add_lv2(std::size_t pos)
{
#if defined(LILV_SHARED)
  auto& world = score::AppComponents().applicationPlugin<Media::ApplicationPlugin>().lilv;

  auto plugs = world.get_all_plugins();

  QStringList items;

  auto it = plugs.begin();
  while(!plugs.is_end(it))
  {
    auto plug = plugs.get(it);
    items.push_back(plug.get_name().as_string());
    it = plugs.next(it);
  }

  auto res = QInputDialog::getItem(
               this,
               tr("Select a plug-in"), tr("Select a LV2 plug-in"),
               items, 0, false);

  if(!res.isEmpty())
  {
    m_dispatcher.submitCommand(
          new Commands::InsertEffect{
            process(),
            Media::LV2::LV2EffectModel::static_concreteKey(),
            res,
            pos});
  }
#endif
}
void InspectorWidget::add_faust(std::size_t pos)
{
#if defined(HAS_FAUST)
  m_dispatcher.submitCommand(
        new Commands::InsertEffect{
          process(),
          FaustEffectFactory::static_concreteKey(),
          "",
          pos});
#endif
}

void InspectorWidget::add_vst2(std::size_t pos)
{
#if defined(HAS_VST2)

  QString defaultPath;
#if defined(__APPLE__)
  defaultPath = "/Library/Audio/Plug-Ins/VST";
#elif defined(__linux__)
  defaultPath = "/usr/lib/vst";
#endif
  auto res = QFileDialog::getOpenFileName(
               this,
               tr("Select a VST plug-in"), defaultPath,
               "VST (*.dll *.so *.vst *.dylib)");

  if(!res.isEmpty())
  {
    m_dispatcher.submitCommand(
          new Commands::InsertEffect{
            process(),
            Media::VST::VSTEffectModel::static_concreteKey(),
            res, pos});
  }
#endif
}

void InspectorWidget::addRequested(int pos)
{
}

int InspectorWidget::cur_pos()
{
  if(m_list->currentRow() >= 0)
    return m_list->currentRow();
  return process().effects().size();
}
struct ListWidgetItem :
    public QObject,
    public QListWidgetItem
{
  public:
    using QListWidgetItem::QListWidgetItem;

};

void InspectorWidget::recreate()
{
  m_list->clear();

  for(const auto& fx : process().effects())
  {
    auto item = new ListWidgetItem(fx.prettyName(), m_list);

    con(fx.metadata(), &score::ModelMetadata::LabelChanged,
        item, [=] (const auto& txt) { item->setText(txt); });
    item->setData(Qt::UserRole, QVariant::fromValue(fx.id()));
    m_list->addItem(item);
  }
}
}
}
