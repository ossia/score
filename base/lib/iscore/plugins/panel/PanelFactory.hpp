#pragma once
#include <iscore/serialization/VisitorInterface.hpp>
#include <QString>

namespace iscore
{
    class PanelPresenter;
    class PanelModel;
    class PanelView;
    class Presenter;
    class DocumentModel;
    class View;

    /**
     * @brief The PanelFactory class
     *
     * Factory for a side panel. Think of the palettes in photoshop.
     */
    class PanelFactory
    {
        public:
            virtual QString panelName() const = 0;
            virtual int panelId() const = 0;

            virtual ~PanelFactory();
            virtual PanelView* makeView(
                    View* parent) = 0;

            virtual PanelPresenter* makePresenter(
                    Presenter* parent_presenter,
                    PanelView* view) = 0;

            virtual PanelModel* makeModel(
                    DocumentModel* parent) = 0;
    };
}
