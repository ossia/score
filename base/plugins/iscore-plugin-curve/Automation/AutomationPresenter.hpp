#pragma once
#include <ProcessInterface/LayerPresenter.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class CurvePresenter;
class QCPGraph;
class LayerView;
class AutomationLayerModel;
class AutomationView;

class AutomationPresenter : public LayerPresenter
{
        Q_OBJECT
    public:
        AutomationPresenter(const LayerModel& model,
                            LayerView* view,
                            QObject* parent);
        ~AutomationPresenter();

        void on_focusChanged() override;

        void setWidth(int width) override;
        void setHeight(int height) override;

        void putToFront() override;
        void putBehind() override;
        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const LayerModel& layerModel() const override;
        const Id<Process>& modelId() const override;

    public slots:
        // From model
        void updateCurve();

    private:
        const AutomationLayerModel& m_viewModel;
        AutomationView* m_view {};

        CurvePresenter* m_curvepresenter{};

        CommandDispatcher<> m_commandDispatcher;
        FocusDispatcher m_focusDispatcher;

        ZoomRatio m_zoomRatio {};
};
