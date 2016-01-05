#include <boost/optional/optional.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QDebug>
#include <QPoint>

#include <Autom3D/Autom3DProcessMetadata.hpp>
#include "Autom3DLayerModel.hpp"
#include "Autom3DModel.hpp"
#include <Process/ModelMetadata.hpp>
#include <State/Address.hpp>
#include <Autom3D/State/Autom3DState.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class QObject;
namespace Autom3D
{
ProcessModel::ProcessModel(
        const TimeValue& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent) :
    Process::ProcessModel {duration, id, ProcessMetadata::processObjectName(), parent},
    m_startState{new ProcessState{*this, 0., this}},
    m_endState{new ProcessState{*this, 1., this}}
{
    pluginModelList = new iscore::ElementPluginModelList{iscore::IDocument::documentContext(*parent), this};

    metadata.setName(QString("Autom3D.%1").arg(*this->id().val()));
    m_handles.emplace_back(-0.5, -0.5, -0.5);
    m_handles.emplace_back(0, 0, 0);
    m_handles.emplace_back(0.5, 0.5, 0.5);
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel {source, id, ProcessMetadata::processObjectName(), parent},
    m_address(source.address()),
    m_min{source.min()},
    m_max{source.max()},
    m_handles{source.handles()},
    m_startState{new ProcessState{*this, 0., this}},
    m_endState{new ProcessState{*this, 1., this}}
{
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);

    metadata.setName(QString("Autom3D.%1").arg(*this->id().val()));
}

Process::ProcessModel* ProcessModel::clone(
        const Id<Process::ProcessModel>& newId,
        QObject* newParent) const
{
    return new ProcessModel {*this, newId, newParent};
}

const ProcessFactoryKey& ProcessModel::key() const
{
    return ProcessMetadata::factoryKey();
}

QString ProcessModel::prettyName() const
{
    return metadata.name() + " : " + address().toString();
}

void ProcessModel::setDurationAndScale(const TimeValue& newDuration)
{
    // We only need to change the duration.
    setDuration(newDuration);
}

void ProcessModel::setDurationAndGrow(const TimeValue& newDuration)
{
    setDuration(newDuration);
}

void ProcessModel::setDurationAndShrink(const TimeValue& newDuration)
{
    setDuration(newDuration);
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

ProcessState* ProcessModel::startStateData() const
{
    return m_startState;
}

ProcessState* ProcessModel::endStateData() const
{
    return m_endState;
}

::State::Address ProcessModel::address() const
{
    return m_address;
}

Point ProcessModel::value(const TimeValue& time)
{
    ISCORE_TODO;
    // TODO instead get a State or at least a MessageList.
    return {};
}

Point ProcessModel::min() const
{
    return m_min;
}

Point ProcessModel::max() const
{
    return m_max;
}

void ProcessModel::setAddress(const ::State::Address &arg)
{
    if(m_address == arg)
    {
        return;
    }

    m_address = arg;
    emit addressChanged(arg);
}

void ProcessModel::setMin(Point arg)
{
    if (m_min == arg)
        return;

    m_min = arg;
    emit minChanged(arg);
}

void ProcessModel::setMax(Point arg)
{
    if (m_max == arg)
        return;

    m_max = arg;
    emit maxChanged(arg);
}
}



void Autom3D::ProcessModel::startExecution()
{
}

void Autom3D::ProcessModel::stopExecution()
{
}

void Autom3D::ProcessModel::reset()
{
}

Selection Autom3D::ProcessModel::selectableChildren() const
{
    return {};
}

Selection Autom3D::ProcessModel::selectedChildren() const
{
    return {};
}

void Autom3D::ProcessModel::setSelection(const Selection& s) const
{
}
