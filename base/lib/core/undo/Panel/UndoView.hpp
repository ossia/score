#pragma once
#include <iscore/plugins/panel/PanelView.hpp>
namespace iscore
{
class CommandStack;
class UndoListWidget;
}

class UndoView final : public iscore::PanelView
{
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;

        explicit UndoView(QObject *v);

        QWidget *getWidget() override;
        const QString shortcut() const override
        { return tr("Ctrl+H"); }
        void setStack(iscore::CommandStack *s);

    private:
        iscore::UndoListWidget *m_list{};
        QWidget *m_widget{};
};
