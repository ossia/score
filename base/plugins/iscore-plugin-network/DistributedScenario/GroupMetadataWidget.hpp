#pragma once
#include <QWidget>

class QComboBox;
#include <iscore/tools/SettableIdentifier.hpp>


namespace Network
{
class Group;
class GroupManager;
class GroupMetadata;
class GroupMetadataWidget : public QWidget
{
        Q_OBJECT
    public:
        GroupMetadataWidget(const GroupMetadata& groupmetadata,
                            const GroupManager* mgr,
                            QWidget* widg);

        void on_groupAdded(const Id<Group>&);
        void on_groupRemoved(const Id<Group>&);

    private slots:
        void on_indexChanged(int);

    private:
        void updateLabel(const Id<Group>& id);

        const GroupMetadata& m_object;
        const GroupManager* m_groups{};
        QComboBox* m_combo{};
};
}
