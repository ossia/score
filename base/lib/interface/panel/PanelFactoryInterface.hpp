#pragma once

namespace iscore
{
	class PanelPresenterInterface;
	class PanelModelInterface;
	class PanelViewInterface;
	class Presenter;
	class Model;
	class View;

	/**
	 * @brief The PanelFactoryInterface class
	 *
	 * Factory for a side panel. Think of the palettes in photoshop.
	 */
	class PanelFactoryInterface
	{
		public:
			virtual PanelViewInterface* makeView(View* parent) = 0;
			virtual PanelPresenterInterface* makePresenter(Presenter* parent_presenter, PanelModelInterface* model, PanelViewInterface* view) = 0;
			virtual PanelModelInterface* makeModel(Model* parent) = 0;
	};
}
