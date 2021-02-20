#include "Process.hpp"

#include <wobjectimpl.h>

W_OBJECT_IMPL(Skeleton::Model)
namespace Skeleton
{

Model::Model(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, "SkeletonProcess", parent}
{
  metadata().setInstanceName(*this);
}

Model::~Model()
{
}

QString Model::prettyName() const noexcept
{
  return tr("Skeleton Process");
}

void Model::setDurationAndScale(const TimeVal& newDuration) noexcept
{
}

void Model::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
}

void Model::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
}
}
template <>
void DataStreamReader::read(const Skeleton::Model& proc)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Skeleton::Model& proc)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Skeleton::Model& proc)
{
}

template <>
void JSONWriter::write(Skeleton::Model& proc)
{
}
