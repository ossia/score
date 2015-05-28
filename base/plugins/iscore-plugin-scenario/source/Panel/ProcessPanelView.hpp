#pragma once
#include <iscore/plugins/panel/PanelView.hpp>

class QGraphicsScene;
class QGraphicsView;
class SizeNotifyingGraphicsView;
class DoubleSlider;
class ProcessPanelView : public iscore::PanelView
{
        Q_OBJECT
        static const iscore::DefaultPanelStatus m_status;
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;

        ProcessPanelView(QObject* parent);

        QWidget* getWidget() override;

        QGraphicsScene* scene() const;
        SizeNotifyingGraphicsView* view() const;

        DoubleSlider* zoomSlider() const
        { return m_zoomSlider; }

    signals:
        void sizeChanged(const QSize&);
        void horizontalZoomChanged(double d);

    private:
        QWidget* m_widget{};
        QGraphicsScene* m_scene{};
        SizeNotifyingGraphicsView* m_view{};
        DoubleSlider* m_zoomSlider{};
};
