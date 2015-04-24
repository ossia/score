#pragma once
#include <QWidget>
#include <iscore/tools/SettableIdentifier.hpp>
class QLabel;
class GroupMetadata;
class Group;

class GroupMetadataWidget : public QWidget
{
    public:
        GroupMetadataWidget(const GroupMetadata& groupmetadata, QWidget* widg);

    private:
        void updateLabel(const id_type<Group>& id);

        QLabel* m_label{};
};
