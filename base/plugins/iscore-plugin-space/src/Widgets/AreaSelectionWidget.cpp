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
    auto lay = new MarginLess<QHBoxLayout>;
    this->setLayout(lay);

    m_comboBox = new QComboBox;
    m_lineEdit = new QLineEdit;
    lay->addWidget(m_comboBox);
    lay->addWidget(m_lineEdit);

    connect(m_lineEdit, &QLineEdit::editingFinished,
            this, &AreaSelectionWidget::lineEditChanged);

    auto& fact = SingletonAreaFactoryList::instance();
    for(auto& elt : fact.factories())
    {
        m_comboBox->addItem(elt->prettyName(), elt->type());
    }
    connect(m_comboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&] (int index) {
        if(index == m_comboBox->findData(GenericAreaModel::static_type()))
        {
            m_lineEdit->setEnabled(true);
        }
        else
        {
            m_lineEdit->setText(fact.factory(m_comboBox->currentData().toInt())->generic_formula());
            m_lineEdit->setEnabled(false);
            lineEditChanged();
        }
    });
}
