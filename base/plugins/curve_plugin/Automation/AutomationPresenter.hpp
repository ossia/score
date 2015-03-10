#pragma once
#include <ProcessInterface/ProcessPresenterInterface.hpp>

class PluginCurveView;
class PluginCurvePresenter;
class PluginCurveModel;

class ProcessViewInterface;
class AutomationViewModel;
class AutomationView;
class ICommandDispatcher;
class AutomationPresenter : public ProcessPresenterInterface
{
        Q_OBJECT
    public:
        AutomationPresenter(ProcessViewModelInterface* model,
                            ProcessViewInterface* view,
                            QObject* parent);

        virtual void setWidth(int width) override;
        virtual void setHeight(int height) override;

        virtual void putToFront() override;
        virtual void putBehind() override;
        virtual void on_zoomRatioChanged(ZoomRatio) override;
        virtual void parentGeometryChanged() override;
        virtual id_type<ProcessViewModelInterface> viewModelId() const override;
        virtual id_type<ProcessSharedModelInterface> modelId() const override;

    public slots:
        // From model
        void on_modelPointsChanged();

    private:
        AutomationViewModel* m_viewModel {};
        AutomationView* m_view {};

        PluginCurveModel* m_curveModel {};
        PluginCurvePresenter* m_curvePresenter {};
        PluginCurveView* m_curveView {};

        ICommandDispatcher* m_commandDispatcher{};

        ZoomRatio m_zoomRatio {};
};
