#pragma once

#include <qstring.h>

namespace iscore
{
    class DocumentModel;
    class PanelModel;
    class PanelPresenter;
    class PanelView;
    class Presenter;
    class View;
    struct ApplicationContext;

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
                    const iscore::ApplicationContext& ctx,
                    iscore::View* parent) = 0;

            virtual PanelPresenter* makePresenter(
                    Presenter* parent_presenter,
                    PanelView* view) = 0;

            virtual PanelModel* makeModel(
                    DocumentModel* parent) = 0;
    };
}
