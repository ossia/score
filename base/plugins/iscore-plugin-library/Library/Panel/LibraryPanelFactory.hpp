#pragma once
#include <iscore/plugins/panel/PanelFactory.hpp>

namespace Library
{
class LibraryPanelFactory : public iscore::PanelFactory
{
    public:
        int panelId() const override;
        QString panelName() const override;
        iscore::PanelView* makeView(
                const iscore::ApplicationContext& ctx,
                QObject*) override;
        iscore::PanelPresenter* makePresenter(
                const iscore::ApplicationContext& ctx,
                iscore::PanelView* view,
                QObject* parent) override;
        iscore::PanelModel* makeModel(
                const iscore::DocumentContext& ctx,
                QObject* parent) override;
};
}
