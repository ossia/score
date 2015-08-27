#include "ComputationsTab.hpp"
#include <iscore/widgets/MarginLess.hpp>
ComputationsTab::ComputationsTab(QWidget *parent):
    QWidget{parent}
{
    auto lay = new iscore::MarginLess<QGridLayout>;
    this->setLayout(lay);

}
