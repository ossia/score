#pragma once
#include <score/gfx/DisplayConfig.hpp>

#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace Gfx::Settings
{
/**
 * @brief Setting up the displays of a machine with no window manager.
 *
 * On an appliance there is no xrandr and no display panel: what the platform
 * reads when it starts is the only say anyone gets. So this is not a normal
 * settings page -- nothing here takes effect until score is restarted, and on
 * a desktop none of it takes effect at all. It says so rather than appearing
 * to work.
 *
 * The outputs come from the kernel, so the list is real on any machine with
 * DRM, including the desktop the appliance is being configured from.
 */
class DisplayConfigDialog final : public QDialog
{
public:
  explicit DisplayConfigDialog(QWidget* parent = nullptr);

private:
  void load();
  void save();
  score::gfx::DisplaySettings collect() const;

  QVector<score::gfx::DisplayOutput> m_outputs;

  QTableWidget* m_table{};
  QCheckBox* m_hwCursor{};
  QCheckBox* m_hideCursor{};
  QCheckBox* m_vertical{};
  QComboBox* m_rotation{};
  QLineEdit* m_headless{};
  QLineEdit* m_device{};
  QSpinBox* m_vkDevice{};
  QSpinBox* m_vkDisplay{};
  QSpinBox* m_vkMode{};
};
}
