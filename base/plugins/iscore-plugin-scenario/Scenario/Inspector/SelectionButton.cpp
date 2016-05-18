#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QIcon>

#include <QPushButton>

#include "SelectionButton.hpp"
#include <iscore/selection/Selection.hpp>
#include <iscore/widgets/SetIcons.hpp>

SelectionButton::SelectionButton(
        const QString &text,
        Selection target,
        iscore::SelectionDispatcher &disp,
        QWidget *parent):
    QWidget{parent},
    m_dispatcher{disp}
{
    auto lay = new iscore::MarginLess<QHBoxLayout>{this};

    QIcon icon;
    makeIcons(&icon, QString(":/icons/next_on.png"), QString(":/icons/next_off.png"));

    m_button = new QPushButton{tr("None")};
    m_button->setObjectName(QString("SelectionButton"));
    m_button->setIcon(icon);
    m_button->setStyleSheet (
                "margin: 5px;"
                "margin-left: 10px;"
                "text-align: left;"
                "text-decoration: underline;"
                "border: none;"
    );
    m_button->setFlat(true);

    m_button->setText(text);
    connect(m_button,  &QPushButton::clicked,
            this, [=] () {
        m_dispatcher.setAndCommit(target);
    });

    lay->addWidget(m_button);
}
