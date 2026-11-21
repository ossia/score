#pragma once
#include <Process/TimeValue.hpp>

#include <Scenario/Application/Menus/MusicalPosition.hpp>

#include <QFont>
#include <QRect>
#include <QString>
#include <QWidget>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
class IntervalModel;

/**
 * @brief The transport's position readout.
 *
 * In musical mode: bar, pulse and quarter of pulse, the time underneath in
 * smaller digits, and a grid with one cell per pulse of the current bar.
 * Otherwise the time alone.
 *
 * The widget has the same size in both modes, so that toggling the mode does
 * not move the rest of the toolbar.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT TransportDisplay final : public QWidget
{
public:
  explicit TransportDisplay(QWidget* parent = nullptr);
  ~TransportDisplay() override;

  /**
   * @brief Show a position.
   * @param date Date in the root interval.
   * @param root Interval whose time signatures apply, if any. Not kept.
   * @param running Whether an execution exists, playing or paused.
   *
   * Only what changed on screen is repainted.
   */
  void setPosition(TimeVal date, const IntervalModel* root, bool running);

  //! The application settings are not loaded yet when the toolbars are
  //! built: whoever owns the mode sets it.
  void setMusical(bool);
  bool musical() const noexcept { return m_musical; }

  QSize sizeHint() const override { return m_size; }
  QSize minimumSizeHint() const override { return m_size; }

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private:
  void applySkin();
  int contentHeight() const;
  void layout();
  void paintGrid(QPainter& p) const;

  QFont m_timeFont;
  QFont m_barsFont;
  QFont m_smallFont;
  QSize m_size;
  QRect m_textRect;
  QRect m_gridRect;
  int m_barsBaseline{};
  int m_smallBaseline{};

  QString m_timeText;
  QString m_barsText;
  int64_t m_milliseconds{-1};
  MusicalPosition m_position{};
  bool m_hasPosition{};
  bool m_running{};
  bool m_musical{};
};
}
