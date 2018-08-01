#include <QPalette>
#include <score_plugin_engine_export.h>
namespace Control
{
SCORE_PLUGIN_ENGINE_EXPORT
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
