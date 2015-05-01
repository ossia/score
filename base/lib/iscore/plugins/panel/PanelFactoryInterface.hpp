#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
#include <QString>

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
            virtual QString panelName() const = 0;
            virtual int panelId() const = 0;

            virtual ~PanelFactoryInterface() = default;
            virtual PanelViewInterface* makeView(
                    View* parent) = 0;

            virtual PanelPresenterInterface* makePresenter(
                    Presenter* parent_presenter,
                    PanelViewInterface* view) = 0;

            virtual PanelModelInterface* makeModel(
                    DocumentModel* parent) = 0;

            virtual PanelModelInterface* loadModel(
                    const VisitorVariant& data,
                    DocumentModel* parent) { return nullptr; }
    };
}
