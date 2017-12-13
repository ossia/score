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
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#endif

#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTEffectModel.hpp>
#endif

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
        auto id = item->data(Qt::UserRole).value<Id<EffectModel>>();
        auto proc = &process().effects().at(id);
        proc->hideUI();
        proc->showUI();
    }, Qt::QueuedConnection);

    recreate();


    // Add an effect
    {
      auto add = new QPushButton{tr("Add (Score)")};
      connect(add, &QPushButton::pressed, this, [&] {

        auto& base_fxs = doc.app.interfaces<EffectFactoryList>();
        auto dialog = new Scenario::AddProcessDialog<EffectFactoryList>(base_fxs, this);

        dialog->on_okPressed = [&] (const auto& proc)  {
          m_dispatcher.submitCommand(
                      new Commands::InsertEffect{process(), proc, "", process().effects().size()});
        };
        dialog->launchWindow();
        dialog->deleteLater();
      });
      lay->addWidget(add);
    }
    {
#if defined(HAS_FAUST)
    m_add = new QPushButton{tr("Add (Faust)")};
    connect(m_add, &QPushButton::pressed,
            this, [=] () {
        m_dispatcher.submitCommand(
                    new Commands::InsertEffect{
                        process(),
                        FaustEffectFactory::static_concreteKey(),
                        "",
                        0});
    });

    lay->addWidget(m_add);
#endif
    }

    {
#if defined(LILV_SHARED)
    auto add_lv2 = new QPushButton{tr("Add (LV2)")};
    connect(add_lv2, &QPushButton::pressed,
            this, [=] () {
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
                            process().effects().size()});
        }
    });

    lay->addWidget(add_lv2);
#endif
    }

#if defined(HAS_VST2)
    {
      auto add_vst = new QPushButton{tr("Add (VST 2)")};
      connect(add_vst, &QPushButton::pressed,
              this, [=] () {

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
                  res,
                  process().effects().size()});
        }
      });

      lay->addWidget(add_vst);
    }
#endif

    // Remove an effect
    // Effects changed
    // Effect list
    // Double-click : open editor window.

    this->setLayout(lay);
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

    for(const auto& fx_id : process().effectsOrder())
    {
        EffectModel& fx = process().effects().at(fx_id);
        auto item = new ListWidgetItem(fx.metadata().getLabel(), m_list);

        con(fx.metadata(), &score::ModelMetadata::LabelChanged,
            item, [=] (const auto& txt) { item->setText(txt); });
        item->setData(Qt::UserRole, QVariant::fromValue(fx_id));
        m_list->addItem(item);
    }
}
}
}
