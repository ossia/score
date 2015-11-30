#pragma once
#include <iscore/plugins/panel/PanelFactory.hpp>
#include <QString>

#include <core/application/ApplicationContext.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <iscore/plugins/panel/PanelView.hpp>

namespace iscore {
class DocumentModel;
class Presenter;
class View;
}  // namespace iscore

class ProcessPanelFactory final : public iscore::PanelFactory
{
    public:
        int panelId() const override;
        QString panelName() const override;

        iscore::PanelView*makeView(
                const iscore::ApplicationContext& ctx,
                iscore::View* parent) override;
        iscore::PanelPresenter*makePresenter(
                iscore::Presenter* parent_presenter,
                iscore::PanelView* view) override;
        iscore::PanelModel*makeModel(
                iscore::DocumentModel* parent) override;
};
