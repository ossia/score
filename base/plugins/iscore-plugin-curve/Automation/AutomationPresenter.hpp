#pragma once
#include <ProcessInterface/ProcessPresenter.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

class CurvePresenter;
class QCPGraph;
class ProcessView;
class AutomationViewModel;
class AutomationView;

class AutomationPresenter : public ProcessPresenter
{
        Q_OBJECT
    public:
        AutomationPresenter(const ProcessViewModel& model,
                            ProcessView* view,
                            QObject* parent);
        ~AutomationPresenter();

        void setWidth(int width) override;
        void setHeight(int height) override;

        void putToFront() override;
        void putBehind() override;
        void on_zoomRatioChanged(ZoomRatio) override;
        void parentGeometryChanged() override;

        const ProcessViewModel& viewModel() const override;
        const id_type<ProcessModel>& modelId() const override;

    public slots:
        // From model
        void on_modelPointsChanged();

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
