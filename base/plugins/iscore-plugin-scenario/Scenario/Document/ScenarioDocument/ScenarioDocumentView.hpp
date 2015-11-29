#pragma once
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>

class BaseGraphicsObject;
class DoubleSlider;
class QGraphicsScene;
class QGraphicsView;
class QObject;
class QWidget;
class ScenarioBaseGraphicsView;
class TimeRulerView;

namespace iscore
{
struct ApplicationContext;
}
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
        BaseGraphicsObject* m_baseObject {};

        QGraphicsView* m_timeRulersView {};
        TimeRulerView* m_timeRuler {};

        DoubleSlider* m_zoomSlider {};
};


