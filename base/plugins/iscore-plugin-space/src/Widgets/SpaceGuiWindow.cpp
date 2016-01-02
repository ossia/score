#include "SpaceGuiWindow.hpp"

#include <iscore/widgets/MarginLess.hpp>

#include "src/SpaceProcess.hpp"
#include "AreaTab.hpp"
#include "SpaceTab.hpp"
#include "ComputationsTab.hpp"

namespace Space
{
SpaceGuiWindow::SpaceGuiWindow(
        const iscore::DocumentContext& ctx,
        const Space::ProcessModel &space,
        QWidget* parent) :
    QWidget{parent}
{
    auto lay = new iscore::MarginLess<QGridLayout>;

    QTabWidget* tabs = new QTabWidget;
    lay->addWidget(tabs);

    this->setLayout(lay);

    tabs->addTab(new AreaTab{ctx, space, this}, tr("Areas"));
    tabs->addTab(new SpaceTab{space.space(), this}, tr("Space"));
    tabs->addTab(new ComputationsTab{this}, tr("Computation"));
}
}
