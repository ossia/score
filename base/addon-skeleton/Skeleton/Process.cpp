#include "Process.hpp"

namespace Skeleton
{

Model::Model(
    const TimeVal& duration,
    const Id<Process::ProcessModel>& id,
    QObject* parent):
  Process::ProcessModel{duration, id, "SkeletonProcess", parent}
{
  metadata().setInstanceName(*this);
}

Model::~Model()
{

}

QString Model::prettyName() const
{
  return tr("Skeleton Process");
}

void Model::startExecution()
{
}

void Model::stopExecution()
{
}

void Model::reset()
{
}

void Model::setDurationAndScale(const TimeVal& newDuration)
{
}

void Model::setDurationAndGrow(const TimeVal& newDuration)
{
}

void Model::setDurationAndShrink(const TimeVal& newDuration)
{
}

}
template <>
void DataStreamReader::read(
    const Skeleton::Model& proc)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(
    Skeleton::Model& proc)
{
  checkDelimiter();
}

template <>
void JSONObjectReader::read(
    const Skeleton::Model& proc)
{
}

template <>
void JSONObjectWriter::write(
    Skeleton::Model& proc)
{
}
