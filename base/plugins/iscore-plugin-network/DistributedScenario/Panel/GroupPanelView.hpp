#pragma once
#include <iscore/plugins/panel/PanelView.hpp>

#include <QString>

class QObject;
class QWidget;


namespace Network
{
class Session;
class GroupManager;

class GroupPanelView : public iscore::PanelView
{
    public:
        const iscore::DefaultPanelStatus& defaultPanelStatus() const override;
        explicit GroupPanelView(QObject* v);

        QWidget* getWidget() override;
        const QString shortcut() const override
        { return tr("Ctrl+G"); }

        void setView(const GroupManager* mgr,
                     const Session* session);

        void setEmptyView();

    private:
        QWidget* m_widget{}, *m_subWidget{};
};
}
