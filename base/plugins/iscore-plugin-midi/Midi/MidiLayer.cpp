#include "MidiLayer.hpp"
#include <Midi/MidiProcess.hpp>

namespace Midi
{
Layer::Layer(
        ProcessModel& model,
        const Id<Process::LayerModel>& id,
        QObject* parent) :
    Process::LayerModel {
        id,
        Metadata<ObjectKey_k, Layer>::get(),
        model,
        parent}
{

}

Layer::Layer(
        const Layer& source,
        ProcessModel& model,
        const Id<Process::LayerModel>& id,
        QObject* parent) :
    Process::LayerModel {
        id,
        Metadata<ObjectKey_k, Layer>::get(),
        model,
        parent}
{
    // Nothing to copy
}

Process::LayerModelPanelProxy* Layer::make_panelProxy(
        QObject* parent) const
{
    return nullptr;
}

void Layer::serialize(
        const VisitorVariant&) const
{
    // Nothing to save
}

const ProcessModel& Layer::model() const
{
    return static_cast<const ProcessModel&>(processModel());
}
}


template<>
void Visitor<Reader<DataStream>>::readFrom(const Midi::Layer& lm)
{
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Midi::Layer& lm)
{
}



template<>
void Visitor<Reader<JSONObject>>::readFrom(const Midi::Layer& lm)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Midi::Layer& lm)
{
}
