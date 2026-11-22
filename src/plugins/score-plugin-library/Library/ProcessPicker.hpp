#pragma once
#include <Process/Dataflow/PortType.hpp>
#include <Process/Preset.hpp>
#include <Process/ProcessFlags.hpp>

#include <QFrame>

#include <score_plugin_library_export.h>

#include <functional>
#include <memory>
#include <optional>

namespace score
{
struct GUIApplicationContext;
}
namespace Library
{
class ProcessesItemModel;

//! What the picker was closed with: a process of the library, and the preset
//! to load in it if one was chosen.
struct PickedProcess
{
  UuidKey<Process::ProcessModel> key;
  QString customData;
  std::optional<Process::Preset> preset;
  //! As the library names it
  QString name;
};

//! The processes to offer are those with a port of this type, an inlet or an
//! outlet: what a cable from the port the picker was opened on can go to.
//! A process that does not say which ports it has is offered.
struct PortFilter
{
  Process::PortType type{};
  bool inlet{};
};

//! What the picker offers, besides the processes with the acceptable flags
struct PickerOptions
{
  //! Only those that can be cabled to a port
  std::optional<PortFilter> ports;
  //! Their presets, for where a preset can be loaded
  bool presets = true;
  //! What the choice will do, shown beside the search when it is not adding
  //! a process: "Replace Gain"
  QString action{};
};

/**
 * @brief The pop-up to choose a process to add: what the library lists.
 *
 * Three columns, the categories, their processes and the presets of the
 * current one, under a search field. Typing searches every category at once:
 * the processes column then lists the matches, best first, with where each one
 * is, and the presets whose name matches under their process.
 *
 * It is driven from the keyboard alone: typing goes to the search, Up and Down
 * move in the current column, Left and Right go from a column to the next,
 * Return adds the process with the chosen preset, Escape closes it.
 */
class SCORE_PLUGIN_LIBRARY_EXPORT ProcessPicker final : public QFrame
{
public:
  using Callback = std::function<void(const PickedProcess&)>;

  //! Opens a picker at `globalPos`, over the window there, for the processes
  //! with one of the `acceptable` flags. `picked` is called once it closed
  //! with a choice; a click elsewhere or Escape closes it without one.
  static ProcessPicker* open(
      const score::GUIApplicationContext& ctx, Process::ProcessFlags acceptable,
      QPoint globalPos, Callback picked, PickerOptions options = {});

  ProcessPicker(
      const score::GUIApplicationContext& ctx, const ProcessesItemModel& library,
      Process::ProcessFlags acceptable, Callback picked, QWidget* parent = nullptr,
      PickerOptions options = {});
  ~ProcessPicker() override;

  //! What is typed in the search field.
  void setSearch(const QString& text);

  //! The names in each column, as shown: for tests.
  QStringList categories() const;
  QStringList processes() const;
  QStringList presets() const;

  //! Closes the picker with the current process and preset, if there is one.
  bool accept();

private:
  bool eventFilter(QObject* obj, QEvent* ev) override;
  void paintEvent(QPaintEvent* ev) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
  void mouseReleaseEvent(QMouseEvent* ev) override;

  struct Impl;
  std::unique_ptr<Impl> m;
  //! Where it was grabbed, from its corner, while it is dragged
  std::optional<QPoint> m_dragFrom;
};
}
