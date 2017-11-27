#include "EffectInspector.hpp"

#include <Media/Effect/Effect/Faust/FaustEffectModel.hpp>
#include <Media/Commands/InsertEffect.hpp>
#include <Media/Commands/EditFaustEffect.hpp>
#include <score/document/DocumentContext.hpp>

#if defined(LILV_SHARED) // TODO instead add a proper preprocessor macro that also works in static case
#include <lilv/lilvmm.hpp>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>
#include <Media/Effect/LV2/LV2EffectModel.hpp>
#include <Media/ApplicationPlugin.hpp>
#endif


#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QInputDialog>

namespace Media
{
namespace Effect
{
struct FaustEditDialog : public QDialog
{
        const FaustEffectModel& m_effect;

        QPlainTextEdit* m_textedit{};
    public:
        FaustEditDialog(const FaustEffectModel& fx):
            m_effect{fx}
        {
            auto lay = new QVBoxLayout;
            this->setLayout(lay);

            m_textedit = new QPlainTextEdit{m_effect.text()};

            lay->addWidget(m_textedit);
            auto bbox = new QDialogButtonBox{
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
            lay->addWidget(bbox);
            connect(bbox, &QDialogButtonBox::accepted,
                    this, &QDialog::accept);
            connect(bbox, &QDialogButtonBox::rejected,
                    this, &QDialog::reject);
        }

        QString text() const
        {
            return m_textedit->document()->toPlainText();
        }
};

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
#if defined(HAS_FAUST)
        if(auto faust = dynamic_cast<FaustEffectModel*>(proc))
        {
            FaustEditDialog edit{*faust};
            auto res = edit.exec();
            if(res)
            {
                m_dispatcher.submitCommand(new Commands::EditFaustEffect{*faust, edit.text()});
            }
        }
#endif
        /*
#if defined(LILV_SHARED)
        if(auto lv2 = dynamic_cast<LV2EffectModel*>(proc))
        {
            // One can take inspiration from Qtractor, and cry a lot.
        }
#endif
        */
    }, Qt::QueuedConnection);

    recreate();

    // Add an effect
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
                            Media::LV2::LV2EffectFactory::static_concreteKey(),
                            res,
                            (int)process().effects().size()});
        }
    });

    lay->addWidget(add_lv2);
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

    /* TODO :
    connect(m_list, &QListWidget::doubleClicked,
            this, [] () {
        // Find factory of effect.
        // Ask it to create a GUI.
    });
    */

}
}
}
