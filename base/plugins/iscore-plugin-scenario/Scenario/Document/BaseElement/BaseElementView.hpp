#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
#include "Widgets/ScenarioBaseGraphicsView.hpp"
#include "Widgets/GraphicsProxyObject.hpp"
class QSlider;
class QGraphicsScene;
class QGraphicsView;
class TemporalConstraintView;
class DoubleSlider;
class TimeRulerView;
class LocalTimeRulerView;
class TimeRuler2;
namespace iscore
{
struct ApplicationContext;
}
class BaseElementView final : public iscore::DocumentDelegateViewInterface
{
        Q_OBJECT

    public:
        BaseElementView(
                const iscore::ApplicationContext& ctx,
                QObject* parent);
        virtual ~BaseElementView() = default;

        QWidget* getWidget() override;

        QGraphicsItem* baseItem() const
        { return m_baseObject;}

        void update();

        QGraphicsScene& scene() const
        { return *m_scene;}

        ScenarioBaseGraphicsView& view() const
        { return *m_view;}

        QGraphicsView* rulerView() const
        { return m_timeRulersView;}

        TimeRulerView* timeRuler()
        { return m_timeRuler;}

        DoubleSlider* zoomSlider() const
        { return m_zoomSlider;}

        void newLocalTimeRuler();

    public slots:

    signals:
        void horizontalZoomChanged(double newZoom);
        void horizontalPositionChanged(int);

    private:
        QWidget* m_widget {};
        QGraphicsScene* m_scene {};
        ScenarioBaseGraphicsView* m_view {};
        GraphicsProxyObject* m_baseObject {};

        QGraphicsView* m_timeRulersView {};
        TimeRulerView* m_timeRuler {};

        DoubleSlider* m_zoomSlider {};
};


