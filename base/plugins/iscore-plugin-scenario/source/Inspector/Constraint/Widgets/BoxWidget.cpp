#include "BoxWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include "LambdaFriendlyQComboBox.hpp"
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>

const QString BoxWidget::hiddenText{ QObject::tr("Hide")};

BoxWidget::BoxWidget(ConstraintInspectorWidget* parent) :
    QWidget {parent},
        m_model {parent->model() },
        m_parent {parent}
{
    QGridLayout* lay = new QGridLayout{this};
    lay->setContentsMargins(1, 1, 0, 0);
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

    // Layout setup
    lay->addWidget(addButton, 0, 0);
    lay->addWidget(addText, 0, 1);
    //lay->addWidget(m_boxList, 1, 0, 1, 2);

}

BoxWidget::~BoxWidget()
{
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
            label = new QLabel{QString::number(vm->id().val().get()), m_comboBoxesWidget};
        }
        auto box = new LambdaFriendlyQComboBox{m_comboBoxesWidget};
        updateComboBox(box, vm);

        lay->addWidget(label, i, 0);
        lay->addWidget(box, i, 1);
        i++;
    }

    m_comboBoxesWidget->setLayout(lay);
    this->layout()->addWidget(m_comboBoxesWidget);
}

void BoxWidget::updateComboBox(LambdaFriendlyQComboBox* combobox, AbstractConstraintViewModel* vm)
{
    combobox->clear();
    combobox->addItem(hiddenText);

    for(const auto& box : m_model->boxes())
    {
        auto id = *box->id().val();
        combobox->addItem(QString::number(id));
        // TODO check that
        if(vm->shownBox() == box->id())
        {
            combobox->setCurrentIndex(combobox->count() - 1);
        }
    }

    connect(combobox, &LambdaFriendlyQComboBox::activated,
            combobox, [=] (QString s) { m_parent->activeBoxChanged(s, vm); });

    connect(vm, &AbstractConstraintViewModel::boxHidden,
            combobox, [=] () { combobox->setCurrentIndex(0); });

    connect(vm, &AbstractConstraintViewModel::boxShown,
            combobox, [=] (id_type<BoxModel> id)
    {
        using namespace std;
        auto elts = combobox->elements();

        for(int i = 0; i < elts.size(); ++i)
        {
            if(elts[i] == QString::number(id.val().get()))
            {
                combobox->setCurrentIndex(i);
                break;
            }
        }
    });
}

void BoxWidget::setModel(const ConstraintModel* m)
{
    m_model = m;
    viewModelsChanged();
}

