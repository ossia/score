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

class LayerModel;
class Process;
class ProcessStateDataInterface;
class QObject;

MappingModel::MappingModel(
        const TimeValue& duration,
        const Id<Process>& id,
        QObject* parent) :
    CurveProcessModel {duration, id, MappingProcessMetadata::processObjectName(), parent}
{
    pluginModelList = new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this};

    // Named shall be enough ?
    setCurve(new CurveModel{Id<CurveModel>(45345), this});

    auto s1 = new DefaultCurveSegmentModel(Id<CurveSegmentModel>(1), m_curve);
    s1->setStart({0., 0.0});
    s1->setEnd({1., 1.});

    m_curve->addSegment(s1);
    connect(m_curve, &CurveModel::changed,
            this, &MappingModel::curveChanged);

    metadata.setName(QString("Mapping.%1").arg(*this->id().val()));
}

MappingModel::MappingModel(
        const MappingModel& source,
        const Id<Process>& id,
        QObject* parent):
    CurveProcessModel{source, id, MappingProcessMetadata::processObjectName(), parent},
    m_sourceAddress(source.sourceAddress()),
    m_targetAddress(source.targetAddress()),
    m_sourceMin{source.sourceMin()},
    m_sourceMax{source.sourceMax()},
    m_targetMin{source.targetMin()},
    m_targetMax{source.targetMax()}
{
    setCurve(source.curve().clone(source.curve().id(), this));
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);
    connect(m_curve, &CurveModel::changed,
            this, &MappingModel::curveChanged);
    metadata.setName(QString("Mapping.%1").arg(*this->id().val()));
}

Process* MappingModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new MappingModel {*this, newId, newParent};
}

const ProcessFactoryKey& MappingModel::key() const
{
    return MappingProcessMetadata::factoryKey();
}


QString MappingModel::prettyName() const
{
    return metadata.name() + " : " + sourceAddress().toString();
}

void MappingModel::setDurationAndScale(const TimeValue& newDuration)
{
    // Whatever happens we want to keep the same curve.
    setDuration(newDuration);
    m_curve->changed();
}

void MappingModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
    m_curve->changed();
}

void MappingModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
    m_curve->changed();
}

LayerModel* MappingModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    auto vm = new MappingLayerModel{*this, viewModelId, parent};
    return vm;
}

LayerModel* MappingModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto vm = new MappingLayerModel {
              static_cast<const MappingLayerModel&>(source), *this, newId, parent};
    return vm;
}


ProcessStateDataInterface* MappingModel::startStateData() const
{
    return nullptr;
}

ProcessStateDataInterface* MappingModel::endStateData() const
{
    return nullptr;
}



iscore::Address MappingModel::sourceAddress() const
{
    return m_sourceAddress;
}

double MappingModel::sourceMin() const
{
    return m_sourceMin;
}

double MappingModel::sourceMax() const
{
    return m_sourceMax;
}

void MappingModel::setSourceAddress(const iscore::Address& arg)
{
    if(m_sourceAddress == arg)
    {
        return;
    }

    m_sourceAddress = arg;
    emit sourceAddressChanged(arg);
    emit m_curve->changed();
}

void MappingModel::setSourceMin(double arg)
{
    if (m_sourceMin == arg)
        return;

    m_sourceMin = arg;
    emit sourceMinChanged(arg);
    emit m_curve->changed();
}

void MappingModel::setSourceMax(double arg)
{
    if (m_sourceMax == arg)
        return;

    m_sourceMax = arg;
    emit sourceMaxChanged(arg);
    emit m_curve->changed();
}



iscore::Address MappingModel::targetAddress() const
{
    return m_targetAddress;
}

double MappingModel::targetMin() const
{
    return m_targetMin;
}

double MappingModel::targetMax() const
{
    return m_targetMax;
}

void MappingModel::setTargetAddress(const iscore::Address& arg)
{
    if(m_targetAddress == arg)
    {
        return;
    }

    m_targetAddress = arg;
    emit targetAddressChanged(arg);
    emit m_curve->changed();
}

void MappingModel::setTargetMin(double arg)
{
    if (m_targetMin == arg)
        return;

    m_targetMin = arg;
    emit targetMinChanged(arg);
    emit m_curve->changed();
}

void MappingModel::setTargetMax(double arg)
{
    if (m_targetMax == arg)
        return;

    m_targetMax = arg;
    emit targetMaxChanged(arg);
    emit m_curve->changed();
}

