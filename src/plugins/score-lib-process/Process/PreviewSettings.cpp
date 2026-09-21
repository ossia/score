#include <Process/PreviewSettings.hpp>

#include <QtGlobal>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::PreviewSettings)

namespace Process
{
PreviewSettings::PreviewSettings()
    : m_enabled{!qEnvironmentVariableIsSet("SCORE_DISABLE_SHADER_PREVIEW")}
{
}

PreviewSettings& PreviewSettings::instance() noexcept
{
  static PreviewSettings self;
  return self;
}

void PreviewSettings::setEnabled(bool b)
{
  if(b == m_enabled)
    return;
  m_enabled = b;
  enabledChanged(b);
}
}
