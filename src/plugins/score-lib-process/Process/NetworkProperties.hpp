#pragma once
#include <Process/ProcessFlags.hpp>
#include <QString>

#include <score_lib_process_export.h>

namespace Process
{
class SCORE_LIB_PROCESS_EXPORT NetworkProperties {
public:
  NetworkProperties();
  virtual ~NetworkProperties();
  QString networkGroup() const noexcept { return m_networkGroup; }
  virtual void setNetworkGroup(const QString& b) = 0;
  virtual void ancestorNetworkGroupChanged() = 0;

  Process::NetworkFlags networkFlags() const noexcept { return m_networkFlags; }
  virtual void setNetworkFlags(Process::NetworkFlags b) = 0;
  virtual void ancestorNetworkFlagsChanged() = 0;

protected:
  QString m_networkGroup{};
  Process::NetworkFlags m_networkFlags{};
};

// More efficient functions are in Scenario (NetworkMetadata.hpp) 
// as they can qobject_cast instead of dyncast ; this is 
// really only to allow this to work with HeaderDelegate
class ProcessModel;
Process::NetworkFlags networkFlags(const Process::ProcessModel& p) noexcept;
QString networkGroup(const Process::ProcessModel& p) noexcept;
}