#pragma once
#include <Curve/Palette/CurveEditionSettings.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

namespace Curve
{

class ApplicationPlugin final : public QObject, public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& ctx);

  ~ApplicationPlugin() override;

  EditionSettings& editionSettings() noexcept { return m_editionSettings; }

  void on_keyPressEvent(QKeyEvent& event) override;
  void on_keyReleaseEvent(QKeyEvent& event) override;

private:
  QActionGroup* m_actions{};
  QAction* m_shiftact{};
  QAction* m_ctrlact{};
  QAction* m_altact{};
  EditionSettings m_editionSettings;
};

}
