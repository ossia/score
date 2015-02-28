#include "BoxWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

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
    lay->setContentsMargins(0, 0, 0, 0);
    this->setLayout(lay);

    // Button
    QToolButton* addButton = new QToolButton{this};
    addButton->setText("+");

    connect(addButton, &QToolButton::pressed,
            [ = ]() { parent->createBox(); });
    // Text
    auto addText = new QLabel{"Add Box", this};
    addText->setStyleSheet(QString{"text-align : left;"});

    // For each view model, a box chooser.
    viewModelsChanged();

    // Current box chooser
    //m_boxList = new QComboBox{this};
    //connect(m_boxList, SIGNAL(activated(QString)),
    //        this, SLOT(on_comboBoxActivated(QString)));


    // Layout setup
    lay->addWidget(addButton, 0, 0);
    lay->addWidget(addText, 0, 1);
    //lay->addWidget(m_boxList, 1, 0, 1, 2);


}

void BoxWidget::viewModelsChanged()
{
    delete m_comboBoxesWidget;
    m_comboBoxesWidget = new QWidget{this};
    auto lay = new QGridLayout;
    int i = 0;

    for(auto vm : m_model->viewModels())
    {
        QLabel* label;
        if(dynamic_cast<FullViewConstraintViewModel*>(vm))
        {
            label = new QLabel{tr("Full view"), m_comboBoxesWidget};
        }
        else
        {
            label = new QLabel{QString::number(vm->id()), m_comboBoxesWidget};
        }
        auto box = new QComboBox{m_comboBoxesWidget};
        updateComboBox(box, vm);

        lay->addWidget(label, i, 0);
        lay->addWidget(box, i, 1);
        i++;
    }

    m_comboBoxesWidget->setLayout(lay);
    this->layout()->addWidget(m_comboBoxesWidget);
}

void BoxWidget::updateComboBox(QComboBox* combobox, AbstractConstraintViewModel* vm)
{
    combobox->clear();
    combobox->addItem(hiddenText);

    for(auto box : m_model->boxes())
    {
        auto id = *box->id().val();
        combobox->addItem(QString::number(id));
        if(vm->shownBox() == id)
        {
            combobox->setCurrentIndex(combobox->count() - 1);
        }
    }
}

void BoxWidget::setModel(ConstraintModel* m)
{
    m_model = m;
    viewModelsChanged();
}

void BoxWidget::on_comboBoxActivated(QString s)
{
    // TODO
    //m_parent->activeBoxChanged(s);
}
