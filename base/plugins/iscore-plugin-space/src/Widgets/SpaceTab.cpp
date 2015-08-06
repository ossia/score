#include "SpaceTab.hpp"
#include <iscore/widgets/MarginLess.hpp>
SpaceTab::SpaceTab(QWidget *parent):
    QWidget{parent}
{
    auto lay = new MarginLess<QGridLayout>;
    this->setLayout(lay);

}
