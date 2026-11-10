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

  //! How many of an editor's own pixels make one of Qt's logical pixels, on
  //! the screen of w. Win32 and X11 editors count physical pixels: this is the
  //! device pixel ratio. Cocoa editors count points, which are Qt's logical
  //! pixels unless Qt scales those itself (QT_SCALE_FACTOR,
  //! QT_SCREEN_SCALE_FACTORS): this is then that scaling alone, whatever the
  //! backing scale of the screen.
  static double editorPixelRatio(const QWidget& w);

  //! The same, from the device pixel ratio and Qt's own scaling
  static constexpr double
  editorPixelRatio(double devicePixelRatio, double qtScaleFactor) noexcept
  {
#if defined(__APPLE__)
    return qtScaleFactor > 0. ? qtScaleFactor : 1.;
#else
    return devicePixelRatio > 0. ? devicePixelRatio : 1.;
#endif
  }

protected:
  //! Escape closes the window as its close button does. QDialog would reject
  //! it instead, which since Qt 6.3 closes it without a closeEvent: the one
  //! place each plug-in host learns that its editor is gone.
  void keyPressEvent(QKeyEvent* event) override;
};
}
