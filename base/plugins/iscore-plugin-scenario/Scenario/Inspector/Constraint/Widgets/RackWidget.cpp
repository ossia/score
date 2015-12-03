#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QObject>

#include <QString>
#include <QStringList>
#include <QToolButton>

#include "LambdaFriendlyQComboBox.hpp"
#include "RackWidget.hpp"
#include <iscore/tools/SettableIdentifier.hpp>

const QString RackWidget::hiddenText{ QObject::tr("Hide")};

RackWidget::RackWidget(ConstraintInspectorWidget* parent) :
    QWidget {parent},
        m_model {parent->model() },
        m_parent {parent}
{
    QVBoxLayout* mainLay = new QVBoxLayout{this};
    QWidget* mainWidg = new QWidget;
    mainLay->addWidget(mainWidg);

    QGridLayout* lay = new QGridLayout{mainWidg};
    lay->setContentsMargins(1, 1, 0, 0);
    mainWidg->setLayout(lay);

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

    lay->addWidget(new QLabel{"View Model"}, i, 0);
    lay->addWidget(new QLabel{"Displayed Rack"}, i, 1);
    i++;

    for(auto vm : m_model.viewModels())
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

    for(const auto& rack : m_model.racks)
    {
        combobox->addItem(rack.metadata.name());
        if(vm->shownRack() == rack.id())
        {
            combobox->setCurrentIndex(combobox->count() - 1);
        }
    }

    connect(combobox, &LambdaFriendlyQComboBox::activated,
            combobox, [=] (QString s) { m_parent->activeRackChanged(s, vm); });

    connect(vm, &ConstraintViewModel::rackHidden,
            combobox, [=] () { combobox->setCurrentIndex(0); });
    connect(vm, &ConstraintViewModel::lastRackRemoved,
            combobox, [=] () { combobox->setCurrentIndex(0); });

    connect(vm, &ConstraintViewModel::rackShown,
            combobox, [=] (Id<RackModel> id)
    {
        using namespace std;
        auto elts = combobox->elements();
        auto r = m_model.racks.find(id);

        for(int i = 0; i < elts.size(); ++i)
        {
            if(elts[i] == (*r).metadata.name())
            {
                combobox->setCurrentIndex(i);
                break;
            }
        }
    });
}
