#pragma once
#include <QDialog>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT PluginWindow : public QDialog
{
public:
  explicit PluginWindow(bool onTop, QWidget* parent);

  ~PluginWindow();
};
}
