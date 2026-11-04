#include <Process/PreviewSettings.hpp>

#include <QSettings>
#include <QtGlobal>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::PreviewSettings)

namespace Process
{
static constexpr auto hoverProbesKey = "Monitoring/HoverProbes";

PreviewSettings::PreviewSettings()
    : m_enabled{!qEnvironmentVariableIsSet("SCORE_DISABLE_SHADER_PREVIEW")}
    , m_hoverProbes{QSettings{}.value(hoverProbesKey, true).toBool()}
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

void PreviewSettings::setHoverProbes(bool b)
{
  if(b == m_hoverProbes)
    return;
  m_hoverProbes = b;
  QSettings{}.setValue(hoverProbesKey, b);
  hoverProbesChanged(b);
}
}
