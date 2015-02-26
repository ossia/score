#pragma once

#include <interface/panel/PanelPresenterInterface.hpp>

class InspectorPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        InspectorPanelPresenter (iscore::Presenter* parent,
                                 iscore::PanelViewInterface* view);

        virtual void on_modelChanged() override;

    private:
        // Connection between the model and the view.
        QMetaObject::Connection m_mvConnection;

};
