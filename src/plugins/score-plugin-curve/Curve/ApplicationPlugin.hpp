#pragma once
#include <Curve/Palette/CurveEditionSettings.hpp>

#include <score_plugin_curve_export.h>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

namespace Curve
{

class SCORE_PLUGIN_CURVE_EXPORT ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& ctx);

  ~ApplicationPlugin() override;

  EditionSettings& editionSettings() noexcept { return m_editionSettings; }

  void on_keyPressEvent(QKeyEvent& event) override;
  void on_keyReleaseEvent(QKeyEvent& event) override;

  //! The tool the held modifiers ask for: Shift sets segments (Alt then
  //! applies to every selected one), else Ctrl creates points, else Alt draws
  //! with the pen; none is selection.
  static Tool toolForModifiers(Qt::KeyboardModifiers mods) noexcept;

private:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void onKey(const QKeyEvent& event, bool pressed);
  void followModifiers(Qt::KeyboardModifiers mods);

  EditionSettings m_editionSettings;
};

}
