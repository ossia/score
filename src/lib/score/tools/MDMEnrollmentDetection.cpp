#include "MDMEnrollmentDetection.hpp"

#include <QProcess>
namespace score
{
bool detectSystemEnrolment()
{
#if defined(__APPLE__)
  QProcess proc;
  proc.setProgram("/usr/bin/profiles");
  proc.setArguments({
      "status",
      "-type",
      "enrollment",
  });
  proc.start();
  proc.waitForFinished();

  auto res = proc.readAllStandardOutput();
  if(res.contains("Enrolled via DEP: No") && res.contains("MDM enrollment: No"))
    return false;

  return true;
#elif defined(_WIN32)
  QProcess proc;
  proc.setProgram("dsregcmd");
  proc.setArguments({"/status"});

  proc.start();
  proc.waitForFinished();

  auto res = proc.readAllStandardOutput();
  if(!res.contains("MdmUrl"))
    return false;

  return true;
#else
  return false;
#endif
}
}
