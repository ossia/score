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

        DoubleSlider* m_zoomSlider {};
};

