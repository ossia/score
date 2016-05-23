#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>

namespace iscore
{
class UndoListWidget;
class UndoPanelDelegate final :
        public iscore::PanelDelegate
{
    public:
        UndoPanelDelegate(
                const iscore::ApplicationContext& ctx);

    private:
        QWidget *widget() override;

        const PanelStatus& defaultPanelStatus() const override;

        void on_modelChanged(
                MaybeDocument oldm,
                MaybeDocument newm) override;

        iscore::UndoListWidget *m_list{};
        QWidget *m_widget{};
};

}
