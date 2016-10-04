#include "UnitWidget.hpp"
#include <brigand/algorithms/for_each.hpp>
#include <iscore/widgets/SignalUtils.hpp>
#include <QComboBox>
#include <QHBoxLayout>

namespace State
{

UnitWidget::UnitWidget(ossia::unit_t u, QWidget* parent):
    QWidget{parent}
{
    m_layout = new QHBoxLayout;
    this->setLayout(m_layout);

    m_dataspace = new QComboBox{this};
    m_unit = new QComboBox{this};
    m_layout->addWidget(m_dataspace);
    m_layout->addWidget(m_unit);

    // Fill dataspace. Unit is filled each time the dataspace changes
    m_dataspace->addItem(tr("None"), QVariant::fromValue(ossia::unit_t{}));
    brigand::for_each<ossia::unit_t>([=] (auto d)
    {
        // For each dataspace, add its text to the combo box
        using dataspace_type = typename decltype(d)::type;
        boost::string_view text = ossia::dataspace_traits<dataspace_type>::text()[0];

        m_dataspace->addItem(
                    QString::fromUtf8(text.data(), text.size()),
                    QVariant::fromValue(ossia::unit_t{dataspace_type{}}));
    });

    // Signals
    connect(m_dataspace, SignalUtils::QComboBox_currentIndexChanged_int(),
            this, [=] (int i) {
        on_dataspaceChanged(m_dataspace->itemData(i).value<ossia::unit_t>());
    });

    connect(m_unit, SignalUtils::QComboBox_currentIndexChanged_int(),
            this, [=] (int i) {
        emit unitChanged(m_unit->itemData(i).value<ossia::unit_t>());
    });

    setUnit(u);

}

ossia::unit_t UnitWidget::unit()
{
    return m_unit->currentData().value<ossia::unit_t>();
}

void UnitWidget::setUnit(ossia::unit_t u)
{
    QSignalBlocker b(this);
    if(u)
    {
        // First update the dataspace combobox
        on_dataspaceChanged(u);

        // Then set the correct unit
        eggs::variants::apply(
               [=] (auto dataspace) { m_unit->setCurrentIndex(dataspace.which()); },
               u);
    }
    else
    {
        // "None" dataspace
        m_dataspace->setCurrentIndex(0);
    }
}

void UnitWidget::on_dataspaceChanged(ossia::unit_t d)
{
    m_unit->clear();

    if(d)
    {
        // Set to default unit, which is the first one for each type list

        // First lift ourselves in the dataspace realm
        eggs::variants::apply([=] (auto dataspace) -> void
        {
            // Then For each unit in the dataspace, add it to the unit combobox.
            brigand::for_each<decltype(dataspace)>([=] (auto u)
            {
                using unit_type = typename decltype(u)::type;
                boost::string_view text = ossia::unit_traits<unit_type>::text()[0];

                m_unit->addItem(
                            QString::fromUtf8(text.data(), text.size()),
                            QVariant::fromValue(ossia::unit_t{unit_type{}}));
            });
        }, d);

        // Update the current unit
        emit unitChanged(m_unit->itemData(0).value<ossia::unit_t>());
    }
    else
    {
        // No unit
        emit unitChanged({});
    }
}

}
