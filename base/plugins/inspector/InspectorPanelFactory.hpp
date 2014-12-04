#pragma once
#include <interface/panel/PanelFactoryInterface.hpp>
#include <interface/panel/PanelModelInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>
#include <interface/panel/PanelViewInterface.hpp>

/**
 * @brief The InspectorPanelModel class
 *
 * Model : keeps the currently identified object. (ObjectPath)
 */
class InspectorPanelModel : public iscore::PanelModelInterface
{
	public:
		InspectorPanelModel (iscore::Model* parent);
};

class InspectorPanelPresenter : public iscore::PanelPresenterInterface
{
	public:
		InspectorPanelPresenter (iscore::Presenter* parent,
		                         iscore::PanelModelInterface* model,
		                         iscore::PanelViewInterface* view);
};

class InspectorPanelView : public iscore::PanelViewInterface
{
	public:
		InspectorPanelView (iscore::View* parent);
		virtual QWidget* getWidget() override;
};


class InspectorPanelFactory : public iscore::PanelFactoryInterface
{


		// PanelFactoryInterface interface
	public:
		virtual iscore::PanelViewInterface* makeView (iscore::View*);
		virtual iscore::PanelPresenterInterface* makePresenter (iscore::Presenter* parent_presenter,
		        iscore::PanelModelInterface* model,
		        iscore::PanelViewInterface* view);
		virtual iscore::PanelModelInterface* makeModel (iscore::Model*);
};
