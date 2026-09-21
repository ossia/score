#pragma once
#include <QObject>

#include <score_lib_process_export.h>

#include <verdigris>

namespace Process
{
/**
 * @brief Whether previews render at all.
 *
 * Shared because the two ends live in different plugins: the graphics plugin
 * owns the toolbar button and the running render nodes, the library panel owns
 * the thumbnails it builds when the selection changes. Both watch this rather
 * than each other.
 */
class SCORE_LIB_PROCESS_EXPORT PreviewSettings final : public QObject
{
  W_OBJECT(PreviewSettings)
public:
  static PreviewSettings& instance() noexcept;

  bool enabled() const noexcept { return m_enabled; }
  void setEnabled(bool b);

  void enabledChanged(bool b) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, enabledChanged, b)

private:
  bool m_enabled{true};
};
}
