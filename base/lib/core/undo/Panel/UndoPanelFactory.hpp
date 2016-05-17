#pragma once

#include <iscore/plugins/panel/PanelFactory.hpp>
#include <QString>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/panel/PanelModel.hpp>
#include <iscore/plugins/panel/PanelPresenter.hpp>
#include <iscore/plugins/panel/PanelView.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>

#include <iscore_lib_base_export.h>


namespace iscore {
class DocumentModel;


}  // namespace iscore
namespace iscore
{
class UndoListWidget;
class UndoPanelDelegate final :
        public iscore::PanelDelegate
{
    public:
        UndoPanelDelegate(
                const iscore::ApplicationContext& ctx,
                QObject* parent);

    private:
        QWidget *widget() override;

        const PanelStatus& defaultPanelStatus() const override;

        void on_modelChanged(
                maybe_document_t oldm,
                maybe_document_t newm) override;

        iscore::UndoListWidget *m_list{};
        QWidget *m_widget{};
};

class ISCORE_LIB_BASE_EXPORT UndoPanelDelegateFactory final :
        public PanelDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("293c0f8b-fcb4-4554-8425-4bc03d803c75")
        public:
            PanelDelegate* make(
                const iscore::ApplicationContext& ctx,
                QObject* parent) override;
};

}

