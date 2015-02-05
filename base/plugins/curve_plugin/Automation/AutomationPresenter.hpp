#pragma once
#include <ProcessInterface/ProcessPresenterInterface.hpp>

class PluginCurveView;
class PluginCurvePresenter;
class PluginCurveModel;

class ProcessViewInterface;
class AutomationViewModel;
class AutomationView;
class AutomationPresenter : public ProcessPresenterInterface
{
		Q_OBJECT
	public:
		AutomationPresenter(ProcessViewModelInterface* model,
							ProcessViewInterface* view,
							QObject* parent);

		virtual void putToFront();
		virtual void putBack();
		virtual void on_horizontalZoomChanged(int);
		virtual void parentGeometryChanged();
		virtual id_type<ProcessViewModelInterface> viewModelId() const;
		virtual id_type<ProcessSharedModelInterface> modelId() const;

	public slots:
		void on_modelPointsChanged();

	private:
		AutomationViewModel* m_viewModel{};
		AutomationView* m_view{};

		PluginCurveModel* m_curveModel{};
		PluginCurvePresenter* m_curvePresenter{};
		PluginCurveView* m_curveView{};

};
