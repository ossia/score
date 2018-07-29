#pragma once
#include <QPixmap>

namespace Process
{
struct Pixmaps
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
