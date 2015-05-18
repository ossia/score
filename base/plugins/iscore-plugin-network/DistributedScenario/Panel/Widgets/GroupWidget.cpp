#include "GroupWidget.hpp"
#include <DistributedScenario/Group.hpp>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

GroupWidget::GroupWidget(Group* group, QWidget* parent):
    QWidget{parent},
    m_group{group}
{
    auto lay = new QHBoxLayout{this};
    lay->addWidget(new QLabel{group->name()});

    auto rename = new QPushButton(QObject::tr("Rename"));
    lay->addWidget(rename);

    auto remove = new QPushButton(QObject::tr("Remove"));
    lay->addWidget(remove);

    // TODO connect add/remove group
}

id_type<Group> GroupWidget::id() const
{
    return m_group->id();
}
