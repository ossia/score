#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Inspector/Constraint/Widgets/ProcessViewTabWidget.hpp>
#include <boost/optional/optional.hpp>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QObject>
#include <QComboBox>

#include <QString>
#include <QStringList>
#include <QToolButton>

#include "RackWidget.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <iscore/widgets/SignalUtils.hpp>
#include <iscore/widgets/SetIcons.hpp>

namespace Scenario
{
const QString RackWidget::hiddenText{ QObject::tr("None")};


RackWidget::RackWidget(ProcessViewTabWidget* parentTabWidget, QWidget* parent) :
    QWidget {parent},
        m_model {parentTabWidget->parentConstraint().model() },
        m_parent {parentTabWidget}
{
    QVBoxLayout* mainLay = new QVBoxLayout{this};
    QWidget* mainWidg = new QWidget;
    mainLay->addWidget(mainWidg);

    auto lay = new iscore::MarginLess<QHBoxLayout>{mainWidg};

    // Button
    QToolButton* addButton = new QToolButton{this};
    addButton->setText("+");
    QIcon addIcon;
    makeIcons(&addIcon, QString(":/icons/condition_add_on.png"), QString(":/icons/condition_add_off.png"));
    addButton->setIcon(addIcon);

    connect(addButton, &QToolButton::pressed,
            [ = ]() { parentTabWidget->createRack(); });
    // Text
    auto addText = new QLabel{"Add Rack", this};
    addText->setStyleSheet(QString{"text-align : left;"});

    // For each view model, a rack chooser.
    viewModelsChanged();

    // Layout setup
    lay->addWidget(addButton);
    lay->addWidget(addText);

    mainLay->addStretch(1);
}

RackWidget::~RackWidget()
{
}

void RackWidget::viewModelsChanged()
{
    delete m_comboBoxesWidget;
    m_comboBoxesWidget = new QWidget{this};
    auto lay = new iscore::MarginLess<QGridLayout>{m_comboBoxesWidget};
    int i = 0;

    auto rackUsed = new QLabel{"Rack Used"};
    rackUsed->setAlignment(Qt::AlignHCenter);

    lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 0);
    lay->addWidget(new QLabel{"View Model"}, i, 1);
    lay->addWidget(rackUsed, i, 2);
    lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 3);
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
//            label = new QLabel{QString::number(vm->id().val().get()), m_comboBoxesWidget};
            //TODO until we have others viewmodel, display a name instead of Id
            label = new QLabel{tr("Reduce view"), m_comboBoxesWidget};
        }
        auto rack = new QComboBox{m_comboBoxesWidget};
        updateComboBox(rack, vm);

        lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 0);
        lay->addWidget(label, i, 1);
        lay->addWidget(rack, i, 2);
        lay->addWidget(new QWidget{m_comboBoxesWidget}, i, 3);
        i++;
    }

    this->layout()->addWidget(m_comboBoxesWidget);
}

void RackWidget::updateComboBox(QComboBox* combobox, ConstraintViewModel* vm)
{
    combobox->clear();
    combobox->addItem(hiddenText);

    for(const auto& rack : m_model.racks)
    {
        combobox->addItem(rack.metadata.name(), QVariant::fromValue(rack.id()));
        if(vm->shownRack() == rack.id())
        {
            combobox->setCurrentIndex(combobox->count() - 1);
        }
    }

    connect(combobox, SignalUtils::QComboBox_activated_int(),
            combobox, [=] (int i) {
        m_parent->activeRackChanged(combobox->itemData(i).value<Id<RackModel>>(), vm);
    });

    connect(vm, &ConstraintViewModel::rackHidden,
            combobox, [=] () { combobox->setCurrentIndex(0); });
    connect(vm, &ConstraintViewModel::lastRackRemoved,
            combobox, [=] () { combobox->setCurrentIndex(0); });

    connect(vm, &ConstraintViewModel::rackShown,
            combobox, [=] (Id<RackModel> id)
    {
        using namespace std;
        int n = combobox->findData(QVariant::fromValue(id));
        if(n != -1)
            combobox->setCurrentIndex(n);
    });
}
}
