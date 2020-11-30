#pragma once
#include <QWidget>

#include <score_lib_process_export.h>

namespace Inspector
{
class Layout;
}
namespace Process
{
class Port;
class Inlet;
class Outlet;
class ControlInlet;
class ControlOutlet;
}
namespace score
{
struct DocumentContext;
}
namespace Process
{
class ProcessModel;

class SCORE_LIB_PROCESS_EXPORT PortWidgetSetup final
{
public:
  static void setupAlone(
      const Process::Port& port,
      const score::DocumentContext& ctx,
      Inspector::Layout& lay,
      QWidget* parent);
  static void setupInLayout(
      const Process::Port& port,
      const score::DocumentContext& ctx,
      Inspector::Layout& lay,
      QWidget* parent);
  static void setupControl(
      const Process::ControlInlet& inlet,
      QWidget* inlet_widget,
      const score::DocumentContext& ctx,
      Inspector::Layout& lay,
      QWidget* parent);
  static void setupControl(
      const Process::ControlOutlet& inlet,
      QWidget* inlet_widget,
      const score::DocumentContext& ctx,
      Inspector::Layout& lay,
      QWidget* parent);

  static QWidget*
  makeAddressWidget(const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent);

private:
  static void setupImpl(
      const QString& txt,
      const Port& port,
      const score::DocumentContext& ctx,
      Inspector::Layout& lay,
      QWidget* parent);
};
/**
 * @brief Show the list of ports / addresses
 *
 * For use in the process inspectors.
 */
class SCORE_LIB_PROCESS_EXPORT PortListWidget final : public QWidget
{
public:
  PortListWidget(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx,
      QWidget* parent);

  void reload();

private:
  const Process::ProcessModel& m_process;
  const score::DocumentContext& m_ctx;
  std::vector<QWidget*> m_inlets;
  std::vector<QWidget*> m_outlets;
};
}
