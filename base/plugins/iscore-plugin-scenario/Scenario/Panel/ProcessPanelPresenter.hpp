#pragma once
#include <Process/ZoomHelper.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
namespace Process
{
class ProcessList;
class LayerModel;
class LayerModelPanelProxy;
}
class ProcessPanelGraphicsProxy;
class QSize;

namespace iscore {
class PanelView;
}  // namespace iscore

class ProcessPanelPresenter final : public iscore::PanelPresenter
{
    public:
        ProcessPanelPresenter(
                const Process::ProcessList& plist,
                iscore::PanelView* view,
                QObject* parent);

        int panelId() const override;
        void on_modelChanged(
                iscore::PanelModel* oldm,
                iscore::PanelModel* newm) override;


    private:
        void on_focusedViewModelChanged(const Process::LayerModel*);

        void cleanup();

        const Process::LayerModel* m_layerModel{};
        Process::LayerModelPanelProxy* m_proxy{};

};
