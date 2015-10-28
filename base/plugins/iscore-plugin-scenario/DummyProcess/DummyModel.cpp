#include "DummyModel.hpp"


Process*DummyModel::clone(const Id<Process>& newId, QObject* newParent) const
{
}

QString DummyModel::processName() const
{
}

QString DummyModel::userFriendlyDescription() const
{
}

QByteArray DummyModel::makeLayerConstructionData() const
{
}

void DummyModel::setDurationAndScale(const TimeValue& newDuration)
{
}

void DummyModel::setDurationAndGrow(const TimeValue& newDuration)
{
}

void DummyModel::setDurationAndShrink(const TimeValue& newDuration)
{
}

void DummyModel::startExecution()
{
}

void DummyModel::stopExecution()
{
}

void DummyModel::reset()
{
}

ProcessStateDataInterface*DummyModel::startState() const
{
}

ProcessStateDataInterface*DummyModel::endState() const
{
}

Selection DummyModel::selectableChildren() const
{
}

Selection DummyModel::selectedChildren() const
{
}

void DummyModel::setSelection(const Selection& s) const
{
}

void DummyModel::serialize(const VisitorVariant& vis) const
{
}

LayerModel*DummyModel::makeLayer_impl(const Id<LayerModel>& viewModelId, const QByteArray& constructionData, QObject* parent)
{
}

LayerModel*DummyModel::loadLayer_impl(const VisitorVariant&, QObject* parent)
{
}

LayerModel*DummyModel::cloneLayer_impl(const Id<LayerModel>& newId, const LayerModel& source, QObject* parent)
{
}
