#include "SelectionButton.hpp"
#include <QHBoxLayout>
#include <QPushButton>
#include <iscore/selection/SelectionDispatcher.hpp>

SelectionButton::SelectionButton(
        const QString &text,
        Selection target,
        iscore::SelectionDispatcher &disp,
        QWidget *parent):
    QWidget{parent},
    m_dispatcher{disp}
{
    auto lay = new QHBoxLayout{this};

    m_button = new QPushButton{tr("None")};
    m_button->setStyleSheet ("text-align: left");
    m_button->setFlat(true);

    m_button->setText(text);
    connect(m_button,  &QPushButton::clicked,
            this, [=] () {
        m_dispatcher.setAndCommit(target);
    });

    lay->addWidget(m_button);
}
