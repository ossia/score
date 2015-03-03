#pragma once

#include <interface/panel/PanelPresenterInterface.hpp>

class InspectorPanelPresenter : public iscore::PanelPresenterInterface
{
    public:
        InspectorPanelPresenter(iscore::Presenter* parent,
                                iscore::PanelViewInterface* view);

        QString modelObjectName() const override
        {
            return "InspectorPanelModel";
        }

        virtual void on_modelChanged() override;

    private:
        QMetaObject::Connection m_mvConnection;

};
