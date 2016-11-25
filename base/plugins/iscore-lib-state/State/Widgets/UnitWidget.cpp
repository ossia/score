#include "UnitWidget.hpp"
#include <brigand/algorithms/for_each.hpp>
#include <iscore/widgets/SignalUtils.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <QComboBox>
#include <QHBoxLayout>

namespace State
{

UnitWidget::UnitWidget(const State::Unit& u, QWidget* parent):
    QWidget{parent}
{
    m_layout = new iscore::MarginLess<QHBoxLayout>{this};

    m_dataspace = new QComboBox{this};
    m_unit = new QComboBox{this};
    m_layout->addWidget(m_dataspace);
    m_layout->addWidget(m_unit);

    // Fill dataspace. Unit is filled each time the dataspace changes
    m_dataspace->addItem(tr("None"), QVariant::fromValue(State::Unit{}));
    brigand::for_each<ossia::unit_t>([=] (auto d)
    {
        // For each dataspace, add its text to the combo box
        using dataspace_type = typename decltype(d)::type;
        boost::string_view text = ossia::dataspace_traits<dataspace_type>::text()[0];

        m_dataspace->addItem(
                    QString::fromUtf8(text.data(), text.size()),
                    QVariant::fromValue(State::Unit{ossia::unit_t{dataspace_type{}}}));
    });

    // Signals
    connect(m_dataspace, SignalUtils::QComboBox_currentIndexChanged_int(),
            this, [=] (int i) {
        on_dataspaceChanged(m_dataspace->itemData(i).value<State::Unit>());
    });

    connect(m_unit, SignalUtils::QComboBox_currentIndexChanged_int(),
            this, [=] (int i) {
        emit unitChanged(m_unit->itemData(i).value<State::Unit>());
    });

    setUnit(u);

}

State::Unit UnitWidget::unit() const
{
    return m_unit->currentData().value<State::Unit>();
}

void UnitWidget::setUnit(const State::Unit& unit)
{
    QSignalBlocker b(this);
    auto& u = unit.get();
    if(u)
    {
        // First update the dataspace combobox
        m_dataspace->setCurrentIndex(u.which() + 1);

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

void UnitWidget::on_dataspaceChanged(const State::Unit& unit)
{
    { QSignalBlocker _{this}; m_unit->clear(); }

    auto& d = unit.get();
    if(d)
    {
        { QSignalBlocker _{this};

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
                            QVariant::fromValue(State::Unit{ossia::unit_t{unit_type{}}}));
            });
        }, d);

        }

        emit unitChanged(m_unit->currentData().value<State::Unit>());
    }
    else
    {
        // No unit
        emit unitChanged({});
    }
}

}
