#pragma once

#include <iscore/plugins/panel/PanelPresenter.hpp>


namespace iscore {
class PanelView;

}  // namespace iscore

namespace InspectorPanel
{
class InspectorPanelPresenter : public iscore::PanelPresenter
{
    public:
        InspectorPanelPresenter(
                iscore::PanelView* view,
                QObject* parent);

        int panelId() const override;

        void on_modelChanged(
                iscore::PanelModel* oldm,
                iscore::PanelModel* newm) override;

    private:
        QMetaObject::Connection m_mvConnection;

};
}
