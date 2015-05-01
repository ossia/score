#pragma once
#include <iscore/plugins/panel/PanelViewInterface.hpp>

class QGraphicsScene;
class QGraphicsView;
class SizeNotifyingGraphicsView;
class ProcessPanelView : public iscore::PanelViewInterface
{
        Q_OBJECT
    public:
        ProcessPanelView(QObject* parent);

        QWidget* getWidget() override;
        Qt::DockWidgetArea defaultDock() const override;
        int priority() const override;

        QGraphicsScene* scene() const;
        SizeNotifyingGraphicsView* view() const;

    signals:
        void sizeChanged(const QSize&);

    private:
        QWidget* m_widget;
        QGraphicsScene* m_scene{};
        SizeNotifyingGraphicsView* m_view{};
};
