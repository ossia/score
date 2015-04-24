#pragma once
#include <QWidget>
#include <iscore/tools/SettableIdentifier.hpp>
class QLabel;
class QComboBox;
class GroupMetadata;
class Group;
class GroupManager;

class GroupMetadataWidget : public QWidget
{
        Q_OBJECT
    public:
        GroupMetadataWidget(const GroupMetadata& groupmetadata,
                            const GroupManager* mgr,
                            QWidget* widg);

        void on_groupAdded(const id_type<Group>&);
        void on_groupRemoved(const id_type<Group>&);

    private slots:
        void on_indexChanged(int);

    private:
        void updateLabel(const id_type<Group>& id);

        const GroupManager* m_groups{};
        QComboBox* m_combo{};
};
