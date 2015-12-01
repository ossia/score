#include "SpaceGuiWindow.hpp"

#include <iscore/widgets/MarginLess.hpp>

#include "src/SpaceProcess.hpp"
#include "AreaTab.hpp"
#include "SpaceTab.hpp"
#include "ComputationsTab.hpp"
SpaceGuiWindow::SpaceGuiWindow(iscore::CommandStackFacade &stack, const SpaceProcess &space, QWidget* parent) :
    QWidget{parent}
{
    auto lay = new iscore::MarginLess<QGridLayout>;

    QTabWidget* tabs = new QTabWidget;
    lay->addWidget(tabs);

    this->setLayout(lay);

    tabs->addTab(new AreaTab{stack, space, this}, tr("Areas"));
    tabs->addTab(new SpaceTab{space.space(), this}, tr("Space"));
    tabs->addTab(new ComputationsTab{this}, tr("Computation"));

}
