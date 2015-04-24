#include <QWidget>
#include <iscore/command/OngoingCommandManager.hpp>

class QTableWidget;
class GroupManager;
class Session;

class GroupTableWidget : public QWidget
{
    public:
        GroupTableWidget(const GroupManager* mgr, const Session* session, QWidget* parent);

        void setup();

    private:
        QTableWidget* m_table{};
        QObject* m_groupConnectionContext{};
        const GroupManager* m_mgr;
        const Session* m_session;

        ObjectPath m_managerPath;
        CommandDispatcher<> m_dispatcher;
};
