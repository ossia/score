#pragma once
#include <Process/StateProcessFactory.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>

namespace Process
{
class SCORE_LIB_PROCESS_EXPORT StateProcessList final
    : public score::InterfaceList<StateProcessFactory>
{
public:
  using object_type = Process::StateProcess;
  ~StateProcessList();
  object_type* loadMissing(const VisitorVariant& vis, QObject* parent) const;
};
}
