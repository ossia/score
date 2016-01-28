#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QBoxLayout>
#include <QIcon>

#include <QPushButton>

#include "SelectionButton.hpp"
#include <iscore/selection/Selection.hpp>

SelectionButton::SelectionButton(
        const QString &text,
        Selection target,
        iscore::SelectionDispatcher &disp,
        QWidget *parent):
    QWidget{parent},
    m_dispatcher{disp}
{
    auto lay = new iscore::MarginLess<QHBoxLayout>;

    m_button = new QPushButton{tr("None")};
    m_button->setIcon(QIcon(":/images/forward.svg"));
    m_button->setStyleSheet (
                "margin: 5px;"
                "margin-left: 10px;"
                "text-align: left;"
                "text-decoration: underline"
    );
    m_button->setFlat(true);

    m_button->setText(text);
    connect(m_button,  &QPushButton::clicked,
            this, [=] () {
        m_dispatcher.setAndCommit(target);
    });

    lay->addWidget(m_button);
    this->setLayout(lay);
}
