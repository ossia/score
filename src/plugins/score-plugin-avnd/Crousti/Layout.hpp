#pragma once
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>

#include <Process/Dataflow/PortFactory.hpp>
namespace score
{
class GraphicsLayout;
}

namespace oscr
{

struct LayoutBuilderBase
{
    QObject& context;
    const Process::Context& doc;
    const Process::PortFactoryList& portFactory;

    const Process::Inlets& inlets;
    const Process::Outlets& outlets;

    score::GraphicsLayout* layout{}; // The current container
    std::vector<score::GraphicsLayout*> createdLayouts{};

    QGraphicsItem* makePortImpl(Process::ControlInlet& portModel, QGraphicsItem& parent);
    QGraphicsItem* makePortImpl(Process::ControlOutlet& portModel, QGraphicsItem& parent);
    QGraphicsItem* makeInlet(int index);
    QGraphicsItem* makeOutlet(int index);
    QGraphicsItem* makeLabel(std::string_view item);

    void finalizeLayout(QGraphicsItem* rootItem);
};

}
