#include "AreaSelectionWidget.hpp"
#include <QLineEdit>
#include <QComboBox>
#include <QHBoxLayout>
#include <iscore/widgets/MarginLess.hpp>

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

    m_comboBox->addItem(AreaModel::pretty_name(), AreaModel::static_type());
    m_comboBox->addItem(CircleAreaModel::pretty_name(), CircleAreaModel::static_type());
    connect(m_comboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [&] (int index) {
        if(index == m_comboBox->findData(AreaModel::static_type()))
        {
            m_lineEdit->setEnabled(true);
        }
        else
        {
            m_lineEdit->setText(CircleAreaModel::formula());
            m_lineEdit->setEnabled(false);
            lineEditChanged();
        }
    });
}




