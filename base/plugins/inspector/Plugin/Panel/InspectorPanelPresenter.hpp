#pragma once

#include <iscore/plugins/panel/PanelPresenterInterface.hpp>

class InspectorPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        InspectorPanelPresenter(iscore::Presenter* parent,
                                iscore::PanelViewInterface* view);

        int panelId() const override;

        virtual void on_modelChanged() override;

    private:
        QMetaObject::Connection m_mvConnection;

};
