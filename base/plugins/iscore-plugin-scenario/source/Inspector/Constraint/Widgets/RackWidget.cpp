#include "RackWidget.hpp"

#include "Inspector/Constraint/ConstraintInspectorWidget.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"

#include "LambdaFriendlyQComboBox.hpp"
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>

const QString RackWidget::hiddenText{ QObject::tr("Hide")};

RackWidget::RackWidget(ConstraintInspectorWidget* parent) :
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
            [ = ]() { parent->createRack(); });
    // Text
    auto addText = new QLabel{"Add Rack", this};
    addText->setStyleSheet(QString{"text-align : left;"});

    // For each view model, a rack chooser.
    viewModelsChanged();

    // Layout setup
    lay->addWidget(addButton, 0, 0);
    lay->addWidget(addText, 0, 1);
    //lay->addWidget(m_rackList, 1, 0, 1, 2);

}

RackWidget::~RackWidget()
{
}

void RackWidget::viewModelsChanged()
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
        auto rack = new LambdaFriendlyQComboBox{m_comboBoxesWidget};
        updateComboBox(rack, vm);

        lay->addWidget(label, i, 0);
        lay->addWidget(rack, i, 1);
        i++;
    }

    m_comboBoxesWidget->setLayout(lay);
    this->layout()->addWidget(m_comboBoxesWidget);
}

void RackWidget::updateComboBox(LambdaFriendlyQComboBox* combobox, ConstraintViewModel* vm)
{
    combobox->clear();
    combobox->addItem(hiddenText);

    for(const auto& rack : m_model->racks())
    {
        auto id = *rack->id().val();
        combobox->addItem(QString::number(id));
        // TODO check that
        if(vm->shownRack() == rack->id())
        {
            combobox->setCurrentIndex(combobox->count() - 1);
        }
    }

    connect(combobox, &LambdaFriendlyQComboBox::activated,
            combobox, [=] (QString s) { m_parent->activeRackChanged(s, vm); });

    connect(vm, &ConstraintViewModel::rackHidden,
            combobox, [=] () { combobox->setCurrentIndex(0); });

    connect(vm, &ConstraintViewModel::rackShown,
            combobox, [=] (id_type<RackModel> id)
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

void RackWidget::setModel(const ConstraintModel* m)
{
    m_model = m;
    viewModelsChanged();
}

