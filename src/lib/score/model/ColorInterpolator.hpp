#pragma once
#include <QColor>

#include <score_lib_base_export.h>
class QPen;
namespace score
{
class SCORE_LIB_BASE_EXPORT ColorBang
{
public:
  const QPen& getNextPen(QColor c1, QColor c2, const QPen& pen) noexcept;
  void start() noexcept;
  void stop() noexcept;
  bool running() const noexcept;

private:
  int8_t pos{};
};
}
