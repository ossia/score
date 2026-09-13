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

  QPixmap show_ui_off;
  QPixmap show_ui_on;

  QPixmap show_script_off;
  QPixmap show_script_on;

  QPixmap preset_off;
  QPixmap preset_on;

  QPixmap record_off;
  QPixmap record_on;

  QPixmap snapshot_off;
  QPixmap snapshot_on;

  QPixmap close_off;
  QPixmap close_on;

  QPixmap nodal_off;
  QPixmap nodal_on;

  QPixmap timeline_off;
  QPixmap timeline_on;

  QPixmap unmuted;
  QPixmap muted;

  QPixmap play;
  QPixmap stop;

  QPixmap unroll;
  QPixmap unroll_selected;
  QPixmap roll;
  QPixmap roll_selected;

  QPixmap unroll_small;
  QPixmap roll_small;

  QPixmap add;
  QPixmap interval_play;
  QPixmap interval_stop;

  QPixmap metricHandle;

  QPixmap portHandleClosed;
  QPixmap portHandleOpen;

private:
  Pixmaps() noexcept;
  ~Pixmaps();

  //! The resource icons pick their @2x variant from the device pixel ratio.
  void reloadIcons();
  //! metricHandle and the port handles are drawn with the skin, at that ratio.
  void reloadSkin();

  int m_loadIndex{-1};
  double m_dpr{};
};
}
