#pragma once
#include <interface/panel/PanelFactoryInterface.hpp>
#include <interface/panel/PanelModelInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>
#include <interface/panel/PanelViewInterface.hpp>

class DeviceExplorerWidget;
class DeviceExplorerModel;
class DeviceExplorerPanelPresenter : public iscore::PanelPresenterInterface
{
	public:
		DeviceExplorerPanelPresenter(iscore::Presenter* parent,
									 iscore::PanelModelInterface* model,
									 iscore::PanelViewInterface* view);
};

class DeviceExplorerPanelModel : public iscore::PanelModelInterface
{
		friend class DeviceExplorerPanelPresenter;
	public:
		DeviceExplorerPanelModel(iscore::Model* parent);

	private:
		DeviceExplorerModel* m_model;
};




class DeviceExplorerPanelView : public iscore::PanelViewInterface
{
		friend class DeviceExplorerPanelPresenter;
	public:
		DeviceExplorerPanelView(iscore::View* parent);
		virtual QWidget* getWidget() override;

		virtual Qt::DockWidgetArea defaultDock() const
		{ return Qt::LeftDockWidgetArea; }

	private:
		DeviceExplorerWidget* m_widget{};
};


class DeviceExplorerPanelFactory : public iscore::PanelFactoryInterface
{
		// PanelFactoryInterface interface
	public:
		virtual iscore::PanelViewInterface* makeView(iscore::View*);
		virtual iscore::PanelPresenterInterface* makePresenter(iscore::Presenter* parent_presenter,
															   iscore::PanelModelInterface* model,
															   iscore::PanelViewInterface* view);
		virtual iscore::PanelModelInterface* makeModel(iscore::Model*);
};
