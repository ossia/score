#pragma once
#include <ProcessInterface/ProcessPresenterInterface.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

class QCustomPlotCurve;
class QCPGraph;
class ProcessViewInterface;
class AutomationViewModel;
class AutomationView;

class AutomationPresenter : public ProcessPresenterInterface
{
        Q_OBJECT
    public:
        AutomationPresenter(const ProcessViewModelInterface& model,
                            ProcessViewInterface* view,
                            QObject* parent);
        ~AutomationPresenter();

        virtual void setWidth(int width) override;
        virtual void setHeight(int height) override;

        virtual void putToFront() override;
        virtual void putBehind() override;
        virtual void on_zoomRatioChanged(ZoomRatio) override;
        virtual void parentGeometryChanged() override;
        virtual const id_type<ProcessViewModelInterface>& viewModelId() const override;
        virtual const id_type<ProcessSharedModelInterface>& modelId() const override;

    public slots:
        // From model
        void on_modelPointsChanged();

    private:
        const AutomationViewModel& m_viewModel;
        AutomationView* m_view {};

        QCustomPlotCurve* m_curve{};

        CommandDispatcher<> m_commandDispatcher;
        FocusDispatcher m_focusDispatcher;

        ZoomRatio m_zoomRatio {};
};
