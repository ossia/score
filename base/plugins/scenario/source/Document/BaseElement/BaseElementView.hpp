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

class BaseElementView : public iscore::DocumentDelegateViewInterface
{
        Q_OBJECT

    public:
        BaseElementView(QObject* parent);
        virtual ~BaseElementView() = default;

        virtual QWidget* getWidget();

        QGraphicsObject* baseObject()
        {
            return m_baseObject;
        }

        void update();

        QGraphicsScene* scene()
        {
            return m_scene;
        }

        SizeNotifyingGraphicsView* view()
        {
            return m_view;
        }

        AddressBar* addressBar()
        {
            return m_addressBar;
        }

        DoubleSlider* zoomSlider()
        {
            return m_zoomSlider;
        }

        TimeRulerView* timeRuler()
        {
            return m_timeRuler;
        }
        TimeRulerView* localTimeRuler()
        {
            return m_localTimeRuler;
        }

    signals:
        void horizontalZoomChanged(double newZoom);
        void verticalZoomChanged(int newZoom);

    private:
        QWidget* m_widget {};
        QGraphicsScene* m_scene {};
        SizeNotifyingGraphicsView* m_view {};
        QGraphicsObject* m_baseObject {};
        TemporalConstraintView* m_constraint {};
        AddressBar* m_addressBar {};
        TimeRulerView* m_timeRuler {};
        TimeRulerView* m_localTimeRuler {};

        DoubleSlider* m_zoomSlider {};
};

