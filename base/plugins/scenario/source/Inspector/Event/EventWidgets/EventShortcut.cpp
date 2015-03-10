#include "EventShortcut.hpp"

#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>


EventShortCut::EventShortCut(QString eventId, QWidget* parent) :
    QWidget {parent}
{
    auto groupLay = new QHBoxLayout{};
    this->setLayout(groupLay);
    this->setContentsMargins(0,0,0,0);
    groupLay->setContentsMargins(5,0,0,0);

    // browser button
    m_eventBtn = new QPushButton{this};
    m_eventBtn->setText(eventId);
    m_eventBtn->setFlat(true);

    m_box = new QCheckBox{};

    groupLay->addWidget(m_eventBtn);
    groupLay->addWidget(m_box);

    connect(m_eventBtn,   &QPushButton::clicked,
            [ = ]()
    {
        emit eventSelected();
    });

}

bool EventShortCut::isChecked()
{
    return m_box->isChecked();
}

QString EventShortCut::eventName()
{
    return m_eventBtn->text();
}
