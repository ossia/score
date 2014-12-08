#pragma once
#include <interface/panel/PanelFactoryInterface.hpp>
#include <interface/panel/PanelModelInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>
#include <interface/panel/PanelViewInterface.hpp>

/**
 * @brief The DeviceExplorerPanelModel class
 *
 * Model : keeps the currently identified object. (ObjectPath)
 */
class DeviceExplorerPanelModel : public iscore::PanelModelInterface
{
	public:
		DeviceExplorerPanelModel(iscore::Model* parent);
};

class DeviceExplorerPanelPresenter : public iscore::PanelPresenterInterface
{
	public:
		DeviceExplorerPanelPresenter(iscore::Presenter* parent,
								iscore::PanelModelInterface* model,
								iscore::PanelViewInterface* view);
};

class DeviceExplorerPanelView : public iscore::PanelViewInterface
{
	public:
		DeviceExplorerPanelView(iscore::View* parent);
		virtual QWidget* getWidget() override;

		virtual Qt::DockWidgetArea defaultDock() const
		{ return Qt::LeftDockWidgetArea; }
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
