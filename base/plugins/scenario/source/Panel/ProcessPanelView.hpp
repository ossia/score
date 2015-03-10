#pragma once
#include <iscore/plugins/panel/PanelViewInterface.hpp>

class QGraphicsScene;
class QGraphicsView;
class ProcessPanelView : public iscore::PanelViewInterface
{
    public:
        ProcessPanelView(QObject* parent);

        QWidget* getWidget() override;
        Qt::DockWidgetArea defaultDock() const override;

        QGraphicsScene* scene() const
        {return m_scene;}


    private:
        QWidget* m_widget;
        QGraphicsScene* m_scene{};
        QGraphicsView* m_view{};
};
