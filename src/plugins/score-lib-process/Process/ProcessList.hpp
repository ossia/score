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

  object_type* loadMissing(
      const UuidKey<Process::ProcessModel>& key, const VisitorVariant& vis,
      const score::DocumentContext& ctx, QObject* parent) const;

  //! The creation counterpart of loadMissing: a command asks for a process
  //! whose factory this build does not have.
  //!
  //! Deserialization can fall back because the bytes are there to keep;
  //! creation has nothing to keep, so this returns a stand-in that reports
  //! itself incomplete. Without it the command asserts and, in a session, that
  //! aborts or throws on every peer built differently from the sender.
  object_type* makeMissing(
      const UuidKey<Process::ProcessModel>& key, const TimeVal& duration,
      const Id<Process::ProcessModel>& id, QObject* parent) const;
};

class SCORE_LIB_PROCESS_EXPORT LayerFactoryList final
    : public score::InterfaceList<LayerFactory>
{
public:
  ~LayerFactoryList();

  //! Resolves the layer for a process. A process standing in for a plug-in we
  //! do not have falls back to a plain layer, so that it can still be shown.
  LayerFactory* findDefaultFactory(const Process::ProcessModel& proc) const;

  //! The factory used for processes no other one claims, if any is registered.
  LayerFactory* fallbackFactory() const;
  LayerFactory* findDefaultFactory(const UuidKey<Process::ProcessModel>& proc) const;
  LayerFactory* get(const UuidKey<Process::ProcessModel>& proc) const
  {
    return findDefaultFactory(proc);
  }
};
}
