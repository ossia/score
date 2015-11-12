#include "AreaSelectionWidget.hpp"
#include <QLineEdit>
#include <QComboBox>
#include <QHBoxLayout>
#include <iscore/widgets/MarginLess.hpp>

#include "src/Area/Generic/GenericAreaModel.hpp"
#include "src/Area/AreaFactory.hpp"
#include "src/Area/SingletonAreaFactoryList.hpp"
#include "src/Area/AreaModel.hpp"
#include "src/Area/Circle/CircleAreaModel.hpp"
AreaSelectionWidget::AreaSelectionWidget(QWidget* parent):
    QWidget{parent}
{
    auto lay = new iscore::MarginLess<QHBoxLayout>;
    this->setLayout(lay);

    m_comboBox = new QComboBox;
    m_lineEdit = new QLineEdit;
    lay->addWidget(m_comboBox);
    lay->addWidget(m_lineEdit);

    connect(m_lineEdit, &QLineEdit::editingFinished,
            this, &AreaSelectionWidget::lineEditChanged);

    auto& fact = SingletonAreaFactoryList::instance();
    for(auto& elt : fact.get())
    {
        m_comboBox->addItem(elt.second->prettyName(), QVariant::fromValue(elt.second->key<AreaFactoryKey>()));
    }
    connect(m_comboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&] (int index) {
        if(index == m_comboBox->findData(GenericAreaModel::static_type()))
        {
            m_lineEdit->setEnabled(true);
        }
        else
        {
            m_lineEdit->setText(fact.get(m_comboBox->currentData().value<AreaFactoryKey>())->generic_formula());
            m_lineEdit->setEnabled(false);
            lineEditChanged();
        }
    });
}
