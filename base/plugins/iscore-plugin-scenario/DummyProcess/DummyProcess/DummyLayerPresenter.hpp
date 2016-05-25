#pragma once
#include <Process/LayerPresenter.hpp>
#include <QPoint>

#include <Process/ZoomHelper.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_lib_dummyprocess_export.h>
#include <Process/Focus/FocusDispatcher.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class QMenu;
class QObject;

namespace Dummy
{
class DummyLayerView;
class ISCORE_LIB_DUMMYPROCESS_EXPORT DummyLayerPresenter final :
        public Process::LayerPresenter
{
    public:
        explicit DummyLayerPresenter(
                const Process::LayerModel& model,
                DummyLayerView* view,
                const Process::ProcessPresenterContext& ctx,
                QObject* parent);

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;

        void putToFront() override;
        void putBehind() override;

        void on_zoomRatioChanged(ZoomRatio) override;

        void parentGeometryChanged() override;

        const Process::LayerModel& layerModel() const override;
        const Id<Process::ProcessModel>& modelId() const override;

        void fillContextMenu(
                QMenu&,
                QPoint pos,
                QPointF scenepos,
                const Process::LayerContextMenuManager&) const override;

    private:
        const Process::LayerModel& m_layer;
        DummyLayerView* m_view{};
};
}
