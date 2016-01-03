#include "ComputationsTab.hpp"
#include <iscore/widgets/MarginLess.hpp>
namespace Space
{
ComputationsTab::ComputationsTab(QWidget *parent):
    QWidget{parent}
{
    auto lay = new iscore::MarginLess<QGridLayout>;
    this->setLayout(lay);
}
}
