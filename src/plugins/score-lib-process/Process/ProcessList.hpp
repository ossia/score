#pragma once
#include <Process/ProcessFactory.hpp>

#include <score/plugins/InterfaceList.hpp>

namespace Process
{
class SCORE_LIB_PROCESS_EXPORT ProcessFactoryList final
    : public score::InterfaceList<ProcessModelFactory>
{
public:
  using object_type = Process::ProcessModel;
  ~ProcessFactoryList();

  object_type* loadMissing(const VisitorVariant& vis, const score::DocumentContext& ctx, QObject* parent) const;
};

class SCORE_LIB_PROCESS_EXPORT LayerFactoryList final
    : public score::InterfaceList<LayerFactory>
{
public:
  ~LayerFactoryList();

  LayerFactory* findDefaultFactory(const Process::ProcessModel& proc) const;
  LayerFactory*
  findDefaultFactory(const UuidKey<Process::ProcessModel>& proc) const;
  LayerFactory* get(const UuidKey<Process::ProcessModel>& proc) const
  {
    return findDefaultFactory(proc);
  }
};
}
