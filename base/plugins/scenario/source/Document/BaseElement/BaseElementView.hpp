#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
#include "Widgets/SizeNotifyingGraphicsView.hpp"
class QSlider;
class QGraphicsScene;
class QGraphicsView;
class TemporalConstraintView;
class AddressBar;
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

        QGraphicsObject* baseObject()
            { return m_baseObject;}

        void update();

        QGraphicsScene* scene()
            { return m_scene;}

        SizeNotifyingGraphicsView* view()
            { return m_view;}

        QGraphicsView* rulerView()
            { return m_timeRulersView;}

        AddressBar* addressBar()
            { return m_addressBar;}

        DoubleSlider* zoomSlider()
            { return m_zoomSlider;}

        TimeRulerView* timeRuler()
            { return m_timeRuler;}
        LocalTimeRulerView* localTimeRuler()
            { return m_localTimeRuler;}

        void newLocalTimeRuler();

        public slots:

    signals:
        void horizontalZoomChanged(double newZoom);
        void verticalZoomChanged(int newZoom);
        void horizontalPositionChanged(int);

    private:
        QWidget* m_widget {};
        QGraphicsScene* m_scene {};
        SizeNotifyingGraphicsView* m_view {};
        QGraphicsObject* m_baseObject {};
        AddressBar* m_addressBar {};
        QGraphicsView* m_timeRulersView {};
        TimeRulerView* m_timeRuler {};
        LocalTimeRulerView* m_localTimeRuler {};

        DoubleSlider* m_zoomSlider {};
};

