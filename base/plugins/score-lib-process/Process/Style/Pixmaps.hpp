#pragma once
#include <QPixmap>

#include <score_lib_process_export.h>

namespace Process
{
struct SCORE_LIB_PROCESS_EXPORT Pixmaps
{
  static const Pixmaps& instance() noexcept;

  const QPixmap show_ui_off;
  const QPixmap show_ui_on;
  const QPixmap rm_process_off;
  const QPixmap rm_process_on;

private:
  Pixmaps();
  ~Pixmaps();
};
}
