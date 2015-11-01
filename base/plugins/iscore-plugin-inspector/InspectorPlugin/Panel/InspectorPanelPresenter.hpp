#pragma once

#include <iscore/plugins/panel/PanelPresenter.hpp>

class InspectorPanelPresenter : public iscore::PanelPresenter
{
    public:
        InspectorPanelPresenter(iscore::Presenter* parent,
                                iscore::PanelView* view);

        int panelId() const override;

        virtual void on_modelChanged() override;

    private:
        QMetaObject::Connection m_mvConnection;

};
