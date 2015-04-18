#include "GroupTableCheckbox.hpp"

#include <QCheckBox>
#include <QHBoxLayout>

GroupTableCheckbox::GroupTableCheckbox()
{
    auto cb = new QCheckBox;
    connect(cb, &QCheckBox::stateChanged, this, &GroupTableCheckbox::stateChanged);

    auto lay = new QHBoxLayout{this};
    lay->addWidget(cb);
    lay->setAlignment(Qt::AlignCenter);
    lay->setContentsMargins(0,0,0,0);
}
