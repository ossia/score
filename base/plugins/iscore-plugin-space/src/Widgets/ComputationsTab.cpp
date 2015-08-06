#include "ComputationsTab.hpp"
#include <iscore/widgets/MarginLess.hpp>
ComputationsTab::ComputationsTab(QWidget *parent):
    QWidget{parent}
{
    auto lay = new MarginLess<QGridLayout>;
    this->setLayout(lay);

}
