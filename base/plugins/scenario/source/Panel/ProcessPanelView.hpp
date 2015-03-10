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

        QGraphicsScene* scene() const
        {return m_scene;}
        SizeNotifyingGraphicsView* view() const
        { return m_view; }

    signals:
        void sizeChanged(const QSize&);

    private:
        QWidget* m_widget;
        QGraphicsScene* m_scene{};
        SizeNotifyingGraphicsView* m_view{};
};
