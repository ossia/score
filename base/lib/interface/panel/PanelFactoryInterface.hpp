#pragma once

namespace iscore
{
    class PanelPresenterInterface;
    class PanelModelInterface;
    class PanelViewInterface;
    class Presenter;
    class DocumentModel;
    class View;

    /**
     * @brief The PanelFactoryInterface class
     *
     * Factory for a side panel. Think of the palettes in photoshop.
     */
    class PanelFactoryInterface
    {
        public:
            virtual ~PanelFactoryInterface() = default;
            virtual PanelViewInterface* makeView (View* parent) = 0;
            virtual PanelPresenterInterface* makePresenter (Presenter* parent_presenter,
                    PanelViewInterface* view) = 0;
            virtual PanelModelInterface* makeModel (DocumentModel* parent) = 0;
    };
}
