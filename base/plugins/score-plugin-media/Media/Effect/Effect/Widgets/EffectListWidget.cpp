#include "EffectListWidget.hpp"

#include <QMimeData>
#include <QDropEvent>
#include <QVBoxLayout>
#include <QFileInfo>
#include <Media/Effect/Effect/Faust/FaustEffectModel.hpp>
#include <Media/Effect/Effect/Widgets/EffectWidget.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <Media/Commands/InsertEffect.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <score/document/DocumentContext.hpp>

#if defined(LILV_SHARED)
#include <lilv/lilvmm.hpp>
#include <Media/Effect/LV2/LV2EffectModel.hpp>
#endif

namespace Media
{
namespace Effect
{
EffectListWidget::EffectListWidget(
        const ProcessModel &fx,
        const score::DocumentContext &doc,
        QWidget *parent):
    QWidget{parent},
    m_effects{fx},
    m_context{doc},
    m_dispatcher{doc.commandStack}
{
    this->setLayout(m_layout = new QHBoxLayout);
    this->setAcceptDrops(true);

    con(fx, &Effect::ProcessModel::effectsChanged,
        this, &EffectListWidget::setup);

    fx.effects().removing.connect<EffectListWidget, &EffectListWidget::on_effectRemoved>(this);

    setup();
}

void EffectListWidget::on_effectRemoved(const EffectModel &fx)
{
    auto it = ossia::find_if(m_widgets, [&] (auto ptr) { return &ptr->effect() == &fx; });
    if(it == m_widgets.end())
        return;

    auto ptr = *it;
    m_widgets.erase(it);
    m_layout->removeWidget(ptr);
    delete ptr;
}

void EffectListWidget::setup()
{
    score::clearLayout(m_layout);
    for(auto& effect : m_effects.effects())
    {
        auto widg = new EffectWidget{effect, m_context, this};
        m_widgets.push_back(widg);
        connect(widg, &EffectWidget::removeRequested, this, [=,&effect] ()
        {
            auto cmd = new Commands::RemoveEffect{m_effects, effect};
            m_dispatcher.submitCommand(cmd);
        }, Qt::QueuedConnection);

        connect(widg, &EffectWidget::sig_performInsert,
                this,  [=,&effect] (
                const Path<Effect::EffectModel>& fx1_p,
                const Path<Effect::EffectModel>& fx2_p,
                bool after)
        {
            auto& fx1 = fx1_p.find(m_context);
            auto& fx2 = fx2_p.find(m_context);
            if(fx1.parent() == fx2.parent() && fx1.parent() == &m_effects)
            {
                int new_pos = m_effects.effectPosition(fx2.id());
                if(after)
                    new_pos++;

                auto cmd = new Commands::MoveEffect{m_effects, fx1.id(), new_pos};
                m_dispatcher.submitCommand(cmd);
            }
        }, Qt::QueuedConnection);
        m_layout->addWidget(widg);
    }

    m_layout->addStretch();
}

void EffectListWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}
void EffectListWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void EffectListWidget::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        auto urls = event->mimeData()->urls();
        int pos = m_effects.effects().size();
        for(auto url : urls)
        {
            auto path = url.toString(QUrl::PreferLocalFile);
            QFileInfo info(path);
            if(info.isFile())
            {
                QFile f{path};
                if(f.open(QIODevice::ReadOnly))
                {
                    if(info.suffix() == "dsp")
                    {
                        QString dat = f.readAll();
                        auto cmd = new Commands::InsertEffect{
                                m_effects,
                                FaustEffectFactory::static_concreteKey(),
                                dat,
                                pos};
                        m_dispatcher.submitCommand(cmd);
                        pos++;
                    }
                }

            }
            else if(info.isDir())
            {

#if defined(LILV_SHARED)
                if(info.suffix() == "lv2")
                { // TODO factory instead.
                    auto cmd = new Commands::InsertEffect{
                            m_effects,
                            Media::LV2::LV2EffectFactory::static_concreteKey(),
                            url.toString(),
                            pos};
                    m_dispatcher.submitCommand(cmd);
                    pos++;

                }
#endif
            }
        }
        event->acceptProposedAction();
    }
}

void EffectListWidget::mousePressEvent(QMouseEvent* event)
{
    emit pressed();
}


}

}
