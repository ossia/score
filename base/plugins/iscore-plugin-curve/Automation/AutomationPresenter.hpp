#pragma once
#include <ProcessInterface/ProcessPresenter.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class CurvePresenter;
class QCPGraph;
class Layer;
class AutomationViewModel;
class AutomationView;

class AutomationPresenter : public ProcessPresenter
{
        Q_OBJECT
    public:
        AutomationPresenter(const LayerModel& model,
                            Layer* view,
                            QObject* parent);
        ~AutomationPresenter();

        void on_focusChanged() override;

        void setWidth(int width) override;
        void setHeight(int height) override;

        void putToFront() override;
        void putBehind() override;
        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const LayerModel& viewModel() const override;
        const id_type<ProcessModel>& modelId() const override;

    public slots:
        // From model
        void updateCurve();

    private:
        const AutomationViewModel& m_viewModel;
        AutomationView* m_view {};

        CurvePresenter* m_curvepresenter{};
        //Curve* m_curve;

//        QCustomPlotCurve* m_curve{};

        CommandDispatcher<> m_commandDispatcher;
        FocusDispatcher m_focusDispatcher;

        ZoomRatio m_zoomRatio {};
};
