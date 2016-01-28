#include <QBoxLayout>
#include <QCheckBox>
#include <QPushButton>

#include "EventShortcut.hpp"
#include <iscore/widgets/MarginLess.hpp>

namespace Scenario
{
EventShortCut::EventShortCut(QString eventId, QWidget* parent) :
    QWidget {parent}
{
    auto groupLay = new iscore::MarginLess<QHBoxLayout>{this};

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
}
