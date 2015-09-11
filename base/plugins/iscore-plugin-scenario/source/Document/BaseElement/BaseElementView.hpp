#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
#include "Widgets/SizeNotifyingGraphicsView.hpp"
#include "Widgets/GraphicsProxyObject.hpp"
class QSlider;
class QGraphicsScene;
class QGraphicsView;
class TemporalConstraintView;
class DoubleSlider;
class TimeRulerView;
class LocalTimeRulerView;

class BaseElementView : public iscore::DocumentDelegateViewInterface
{
        Q_OBJECT

    public:
        BaseElementView(QObject* parent);
        virtual ~BaseElementView() = default;

        virtual QWidget* getWidget();

        QGraphicsItem* baseItem()
            { return m_baseObject;}

        void update();

        QGraphicsScene* scene()
            { return m_scene;}

        SizeNotifyingGraphicsView* view()
            { return m_view;}

        QGraphicsView* rulerView()
            { return m_timeRulersView;}

        TimeRulerView* timeRuler()
            { return m_timeRuler;}
/*        LocalTimeRulerView* localTimeRuler()
            { return m_localTimeRuler;}

*/
        DoubleSlider* zoomSlider()
            { return m_zoomSlider;}

        void newLocalTimeRuler();

    public slots:

    signals:
        void horizontalZoomChanged(double newZoom);
        void horizontalPositionChanged(int);

    private:
        QWidget* m_widget {};
        QGraphicsScene* m_scene {};
        SizeNotifyingGraphicsView* m_view {};
        GraphicsProxyObject* m_baseObject {};

        QGraphicsView* m_timeRulersView {};
        TimeRulerView* m_timeRuler {};
        //LocalTimeRulerView* m_localTimeRuler {};

        DoubleSlider* m_zoomSlider {};
};

