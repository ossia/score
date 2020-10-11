#pragma once
#include <QPixmap>

#include <score_lib_process_export.h>

namespace Process
{
// TODO consider to change them into images,
// since it is a tiny bit more efficient on the raster paint backend
// TODO reconsider that with RHI in Qt6...
struct SCORE_LIB_PROCESS_EXPORT Pixmaps
{
  static const Pixmaps& instance() noexcept;

  const QPixmap show_ui_off;
  const QPixmap show_ui_on;

  const QPixmap close_off;
  const QPixmap close_on;

  const QPixmap nodal_off;
  const QPixmap nodal_on;

  const QPixmap timeline_off;
  const QPixmap timeline_on;

  const QPixmap unmuted;
  const QPixmap muted;

  const QPixmap play;
  const QPixmap stop;

  const QPixmap unroll;
  const QPixmap unroll_selected;
  const QPixmap roll;
  const QPixmap roll_selected;

  const QPixmap unroll_small;
  const QPixmap roll_small;

  const QPixmap add;

  const QPixmap metricHandle;

  const QPixmap portHandleClosed;
  const QPixmap portHandleOpen;

private:
  Pixmaps() noexcept;
  ~Pixmaps();
};
}
