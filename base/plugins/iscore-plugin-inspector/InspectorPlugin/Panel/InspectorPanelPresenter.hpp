#pragma once

#include <iscore/plugins/panel/PanelPresenter.hpp>


namespace iscore {
class PanelView;

}  // namespace iscore

class InspectorPanelPresenter : public iscore::PanelPresenter
{
    public:
        InspectorPanelPresenter(
                iscore::PanelView* view,
                QObject* parent);

        int panelId() const override;

        void on_modelChanged() override;

    private:
        QMetaObject::Connection m_mvConnection;

};
