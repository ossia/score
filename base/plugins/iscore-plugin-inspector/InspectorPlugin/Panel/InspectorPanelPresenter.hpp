#pragma once

#include <iscore/plugins/panel/PanelPresenter.hpp>


namespace iscore {
class PanelView;
class Presenter;
}  // namespace iscore

class InspectorPanelPresenter : public iscore::PanelPresenter
{
    public:
        InspectorPanelPresenter(iscore::Presenter* parent,
                                iscore::PanelView* view);

        int panelId() const override;

        void on_modelChanged() override;

    private:
        QMetaObject::Connection m_mvConnection;

};
