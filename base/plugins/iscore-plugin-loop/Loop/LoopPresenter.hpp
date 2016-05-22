#pragma once
#include <Loop/LoopViewUpdater.hpp>
#include <Loop/Palette/LoopToolPalette.hpp>
#include <Process/LayerPresenter.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <QDebug>
#include <QPoint>

#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ZoomHelper.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class QMenu;
class QObject;
namespace Scenario
{
class TemporalConstraintPresenter;
}
namespace Loop {
class Layer;
class LayerView;
class ProcessModel;
}  // namespace Loop
namespace iscore {
class CommandStackFacade;
struct DocumentContext;
}  // namespace iscore


namespace Loop
{
inline void removeSelection(const Loop::ProcessModel& model, const iscore::CommandStackFacade& )
{

}

inline void clearContentFromSelection(const Loop::ProcessModel& model, const iscore::CommandStackFacade& )
{
    ISCORE_TODO;
}
}

namespace Loop
{
class LayerPresenter final :
        public Process::LayerPresenter,
        public BaseScenarioPresenter<Loop::ProcessModel, Scenario::TemporalConstraintPresenter>
{
        Q_OBJECT
        friend class ViewUpdater;
    public:
        LayerPresenter(
                const Loop::Layer&,
                LayerView* view,
                const Process::ProcessPresenterContext& ctx,
                QObject* parent);

        ~LayerPresenter();
        LayerView& view() const
        { return *m_view; }

        using BaseScenarioPresenter<Loop::ProcessModel, Scenario::TemporalConstraintPresenter>::event;
        using QObject::event;

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const Process::LayerModel& layerModel() const override;
        const Id<Process::ProcessModel>& modelId() const override;

        ZoomRatio zoomRatio() const
        { return m_zoomRatio; }

        void fillContextMenu(QMenu*, const QPoint& pos, const QPointF& scenepos) const override;

    signals:
        void pressed(QPointF);
        void moved(QPointF);
        void released(QPointF);
        void escPressed();

    private:
        const Loop::Layer& m_layer;
        LayerView* m_view{};

        ZoomRatio m_zoomRatio {};

        ViewUpdater m_viewUpdater;

        ToolPalette m_palette;
        void updateAllElements();
};
}
