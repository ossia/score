#include <QPalette>

namespace Control
{
const QPalette& transparentPalette()
{
  static QPalette p{[] {
    QPalette palette;
    palette.setBrush(QPalette::Background, Qt::transparent);
    return palette;
  }()};
  return p;
}
}
