#pragma once
#include <utility>
#include <score_lib_base_export.h>
class QGraphicsItem;
namespace score
{

class SCORE_LIB_BASE_EXPORT ItemBounder
{
public:
  // Returns: new X, boolean indicating if we moved
  std::pair<double, bool> bound(QGraphicsItem* parent, double x0, double w) noexcept;

  double x() const noexcept { return m_x; }

private:
  double m_x{};
};

}
