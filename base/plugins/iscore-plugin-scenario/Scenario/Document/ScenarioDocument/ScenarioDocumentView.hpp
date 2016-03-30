#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>

class BaseGraphicsObject;
class DoubleSlider;
class QGraphicsScene;
class QGraphicsView;
class QObject;
class QWidget;
class ProcessGraphicsView;

namespace iscore
{
struct ApplicationContext;
}

namespace Scenario
{
class TimeRulerView;
class ScenarioDocumentView final : public iscore::DocumentDelegateViewInterface
{
        Q_OBJECT

    public:
        ScenarioDocumentView(
                const iscore::ApplicationContext& ctx,
                QObject* parent);
        virtual ~ScenarioDocumentView() = default;

        QWidget* getWidget() override;

        BaseGraphicsObject* baseItem() const
        { return m_baseObject;}

        void update();

        QGraphicsScene& scene() const
        { return *m_scene;}

        ProcessGraphicsView& view() const
        { return *m_view;}

        QGraphicsView* rulerView() const
        { return m_timeRulersView;}

        TimeRulerView* timeRuler()
        { return m_timeRuler;}

        DoubleSlider* zoomSlider() const
        { return m_zoomSlider;}

        void newLocalTimeRuler();

    signals:
        void horizontalZoomChanged(double newZoom);
        void horizontalPositionChanged(int);
        void elementsScaleChanged(double);

    private:
        QWidget* m_widget {};
        QGraphicsScene* m_scene {};
        ProcessGraphicsView* m_view {};
        BaseGraphicsObject* m_baseObject {};

        QGraphicsView* m_timeRulersView {};
        TimeRulerView* m_timeRuler {};

        DoubleSlider* m_zoomSlider {};
};
}
