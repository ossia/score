#pragma once

#include <iscore/plugins/panel/PanelFactory.hpp>
#include <QString>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <iscore/plugins/panel/PanelView.hpp>

#include <iscore_lib_base_export.h>
namespace iscore {
class DocumentModel;


}  // namespace iscore

class ISCORE_LIB_BASE_EXPORT UndoPanelFactory final : public iscore::PanelFactory
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
