#pragma once
#include <QWidget>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <nano_observer.hpp>
#include <score/model/path/Path.hpp>
namespace Scenario
{
}
namespace score
{
struct DocumentContext;
}
class QHBoxLayout;
namespace Media
{
namespace Effect
{
class EffectWidget;
class ProcessModel;
class EffectModel;
class EffectListWidget final :
        public QWidget,
        public Nano::Observer
{
        Q_OBJECT
    public:
        EffectListWidget(
                const Effect::ProcessModel& fx,
                const score::DocumentContext& doc,
                QWidget* parent);

        void on_effectRemoved(const EffectModel& fx);

        void setup();

    signals:
        void pressed();

    private:
        void mousePressEvent(QMouseEvent* event) override;
        void dragEnterEvent(QDragEnterEvent* event) override;
        void dragMoveEvent(QDragMoveEvent* event) override;
        void dropEvent(QDropEvent *event) override;

        const Effect::ProcessModel& m_effects;
        const score::DocumentContext& m_context;
        CommandDispatcher<> m_dispatcher;

        QHBoxLayout* m_layout{};

        std::vector<EffectWidget*> m_widgets;
};
}
}
