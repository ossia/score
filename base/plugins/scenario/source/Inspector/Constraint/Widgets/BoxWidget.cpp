#include "BoxWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"


#include <QtWidgets/QGridLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QComboBox>

BoxWidget::BoxWidget(ConstraintInspectorWidget* parent) :
    QWidget {parent},
        m_model {parent->model() },
m_parent {parent}
{
    QGridLayout* lay = new QGridLayout{this};
    lay->setContentsMargins(0, 0, 0 , 0);
    this->setLayout(lay);

    // Button
    QToolButton* addButton = new QToolButton{this};
    addButton->setText("+");

    // Text
    auto addText = new QLabel{"Add Box", this};
    addText->setStyleSheet(QString{"text-align : left;"});

    // Current box chooser
    m_boxList = new QComboBox{this};
    connect(m_boxList, SIGNAL(activated(QString)),
            this, SLOT(on_comboBoxActivated(QString)));


    // Layout setup
    lay->addWidget(addButton, 0, 0);
    lay->addWidget(addText, 0, 1);
    lay->addWidget(m_boxList, 1, 0, 1, 2);

    connect(addButton, &QToolButton::pressed,
            [ = ]()
    {
        parent->createBox();
    });
}

void BoxWidget::updateComboBox()
{
    m_boxList->clear();
    m_boxList->addItem(hiddenText);

    if(m_model)
    {
        auto boxesPtrs = m_model->boxes();

        for(auto box : boxesPtrs)
        {
            m_boxList->addItem(QString::number(*box->id().val()));
        }
    }

    m_boxList->setCurrentIndex(m_boxList->count() - 1);
    on_comboBoxActivated(m_boxList->itemText(m_boxList->count() - 1));
}

void BoxWidget::setModel(ConstraintModel* m)
{
    m_model = m;
}

void BoxWidget::on_comboBoxActivated(QString s)
{
    m_parent->activeBoxChanged(s);
}
