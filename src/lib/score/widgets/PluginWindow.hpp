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

protected:
  //! Escape closes the window as its close button does. QDialog would reject
  //! it instead, which since Qt 6.3 closes it without a closeEvent: the one
  //! place each plug-in host learns that its editor is gone.
  void keyPressEvent(QKeyEvent* event) override;
};
}
