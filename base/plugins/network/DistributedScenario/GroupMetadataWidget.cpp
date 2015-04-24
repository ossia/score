#include "GroupMetadataWidget.hpp"

#include <QVBoxLayout>
#include <QLabel>
#include "GroupMetadata.hpp"
#include "Group.hpp"

GroupMetadataWidget::GroupMetadataWidget(const GroupMetadata& groupmetadata, QWidget* widg):
    QWidget{widg}
{
    this->setLayout(new QVBoxLayout);

    connect(&groupmetadata, &GroupMetadata::groupChanged,
            this, [=] (const id_type<Group>& grp)
    {
        updateLabel(grp);
    });

    updateLabel(groupmetadata.id());
}

void GroupMetadataWidget::updateLabel(const id_type<Group>& id)
{
    delete m_label;
    m_label = new QLabel{"Group: " + QString::number(id.val().get())};

    this->layout()->addWidget(m_label);
}
