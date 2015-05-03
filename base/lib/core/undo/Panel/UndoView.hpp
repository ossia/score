#pragma once
#include <iscore/plugins/panel/PanelView.hpp>
namespace iscore
{
class CommandStack;
class UndoListWidget;
}

class UndoView : public iscore::PanelView
{
    public:
        UndoView(QObject *v);

        QWidget *getWidget() override;
        Qt::DockWidgetArea defaultDock() const override;
        int priority() const override;
        QString prettyName() const override;

        void setStack(iscore::CommandStack *s);

    private:
        iscore::UndoListWidget *m_list{};
        QWidget *m_widget{};
};
