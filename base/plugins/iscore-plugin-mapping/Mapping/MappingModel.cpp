#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Curve/Process/CurveProcessModel.hpp>
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Mapping/MappingProcessMetadata.hpp>
#include "MappingLayerModel.hpp"
#include "MappingModel.hpp"
#include <Process/ModelMetadata.hpp>
#include <State/Address.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QObject;

namespace Mapping
{
ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent) :
    Curve::CurveProcessModel {duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
    pluginModelList = new iscore::ElementPluginModelList{iscore::IDocument::documentContext(*parent), this};

    // Named shall be enough ?
    setCurve(new Curve::Model{Id<Curve::Model>(45345), this});

    auto s1 = new Curve::DefaultCurveSegmentModel(Id<Curve::SegmentModel>(1), m_curve);
    s1->setStart({0., 0.0});
    s1->setEnd({1., 1.});

    m_curve->addSegment(s1);
    connect(m_curve, &Curve::Model::changed,
            this, &ProcessModel::curveChanged);

    metadata.setName(QString("Mapping.%1").arg(*this->id().val()));
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    CurveProcessModel{source, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent},
    m_sourceAddress(source.sourceAddress()),
    m_targetAddress(source.targetAddress()),
    m_sourceMin{source.sourceMin()},
    m_sourceMax{source.sourceMax()},
    m_targetMin{source.targetMin()},
    m_targetMax{source.targetMax()}
{
    setCurve(source.curve().clone(source.curve().id(), this));
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);
    connect(m_curve, &Curve::Model::changed,
            this, &ProcessModel::curveChanged);
    metadata.setName(QString("Mapping.%1").arg(*this->id().val()));
}

Process::ProcessModel* ProcessModel::clone(
        const Id<Process::ProcessModel>& newId,
        QObject* newParent) const
{
    return new ProcessModel {*this, newId, newParent};
}

ProcessFactoryKey ProcessModel::concreteFactoryKey() const
{
    return Metadata<ConcreteFactoryKey_k, ProcessModel>::get();
}


QString ProcessModel::prettyName() const
{
    return metadata.name() + " : \n  " + sourceAddress().toString() + " ->\n  " + targetAddress().toString();
}

void ProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    // Whatever happens we want to keep the same curve.
    setDuration(newDuration);
    m_curve->changed();
}

void ProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
    m_curve->changed();
}

void ProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
    m_curve->changed();
}

Process::LayerModel* ProcessModel::makeLayer_impl(
        const Id<Process::LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto vm = new LayerModel{*this, viewModelId, parent};
    return vm;
}

Process::LayerModel* ProcessModel::cloneLayer_impl(
        const Id<Process::LayerModel>& newId,
        const Process::LayerModel& source,
        QObject* parent)
{
    auto vm = new LayerModel {
              static_cast<const LayerModel&>(source), *this, newId, parent};
    return vm;
}


ProcessStateDataInterface* ProcessModel::startStateData() const
{
    return nullptr;
}

ProcessStateDataInterface* ProcessModel::endStateData() const
{
    return nullptr;
}



State::Address ProcessModel::sourceAddress() const
{
    return m_sourceAddress;
}

double ProcessModel::sourceMin() const
{
    return m_sourceMin;
}

double ProcessModel::sourceMax() const
{
    return m_sourceMax;
}

void ProcessModel::setSourceAddress(const State::Address& arg)
{
    if(m_sourceAddress == arg)
    {
        return;
    }

    m_sourceAddress = arg;
    emit sourceAddressChanged(arg);
    emit m_curve->changed();
}

void ProcessModel::setSourceMin(double arg)
{
    if (m_sourceMin == arg)
        return;

    m_sourceMin = arg;
    emit sourceMinChanged(arg);
    emit m_curve->changed();
}

void ProcessModel::setSourceMax(double arg)
{
    if (m_sourceMax == arg)
        return;

    m_sourceMax = arg;
    emit sourceMaxChanged(arg);
    emit m_curve->changed();
}



State::Address ProcessModel::targetAddress() const
{
    return m_targetAddress;
}

double ProcessModel::targetMin() const
{
    return m_targetMin;
}

double ProcessModel::targetMax() const
{
    return m_targetMax;
}

void ProcessModel::setTargetAddress(const State::Address& arg)
{
    if(m_targetAddress == arg)
    {
        return;
    }

    m_targetAddress = arg;
    emit targetAddressChanged(arg);
    emit m_curve->changed();
}

void ProcessModel::setTargetMin(double arg)
{
    if (m_targetMin == arg)
        return;

    m_targetMin = arg;
    emit targetMinChanged(arg);
    emit m_curve->changed();
}

void ProcessModel::setTargetMax(double arg)
{
    if (m_targetMax == arg)
        return;

    m_targetMax = arg;
    emit targetMaxChanged(arg);
    emit m_curve->changed();
}
}
