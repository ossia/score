#pragma once
#include <QString>
#include <QWidget>

#include <score_lib_process_export.h>

class QLabel;
namespace score
{
class ArrowButton;
}

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

/**
 * @brief Whether the cables of a port keep its address from taking effect.
 *
 * The execution engine reads the address of an inlet only when it has no
 * cable, or when the process at the other end of one of its immediate glutton
 * cables is not playing. An outlet writes to its address under the same
 * condition on its outgoing cables; a propagated audio outlet has a cable to
 * its parent interval, which plays whenever the process does. Texture and
 * geometry ports use their address next to their cables.
 */
struct SCORE_LIB_PROCESS_EXPORT AddressStatus
{
  enum Kind
  {
    //! No address, or the address is used
    Used,
    //! Used only while a process at the other end of a cable is not playing
    WhileCablesIdle,
    //! Never used while the cables are there
    Unused
  } kind{Used};

  //! Explanation for the user, empty when kind is Used
  QString reason;

  static AddressStatus of(const Process::Port& port, const score::DocumentContext& ctx);
};

class SCORE_LIB_PROCESS_EXPORT PortWidgetSetup final
{
public:
  //! A collapsible group of rows shown under a port
  struct Drawer
  {
    QWidget* widget{};
    Inspector::Layout* layout{};
    score::ArrowButton* button{};

    void setOpen(bool open) const;
  };

  /**
   * @brief Adds the row of a port: its name, then `main` (the address picker,
   * a control...).
   *
   * The drawer it returns is not added: the caller fills its layout, ending
   * with makeCablesWidget so that the cables of the port are listed inside it,
   * and adds its widget where the settings go.
   */
  static Drawer setupWithDrawer(
      const Process::Port& port, QWidget* main, const score::DocumentContext& ctx,
      Inspector::Layout& lay, QWidget* parent);

  //! The row of a port whose only setting is the address picker, and its cables.
  static void setupAddressPort(
      const Process::Port& port, const score::DocumentContext& ctx,
      Inspector::Layout& lay, QWidget* parent);

  //! The cables of a port, each with a link to the other end; hidden without cables.
  static QWidget* makeCablesWidget(
      const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent);

  //! `address_widget` followed by an indicator shown when AddressStatus says
  //! the cables bypass the address.
  static QWidget* withAddressStatus(
      const Process::Port& port, QWidget* address_widget,
      const score::DocumentContext& ctx, QWidget* parent);

  static void setupAlone(
      const Process::Port& port, const score::DocumentContext& ctx,
      Inspector::Layout& lay, QWidget* parent);
  static void setupInLayout(
      const Process::Port& port, const score::DocumentContext& ctx,
      Inspector::Layout& lay, QWidget* parent);
  static void setupControl(
      const Process::ControlInlet& inlet, QWidget* inlet_widget,
      const score::DocumentContext& ctx, Inspector::Layout& lay, QWidget* parent);
  static void setupControl(
      const Process::ControlOutlet& inlet, QWidget* inlet_widget,
      const score::DocumentContext& ctx, Inspector::Layout& lay, QWidget* parent);

  static QWidget* makeAddressWidget(
      const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent);

  //! A checkbox and the scripting name under which the port is published.
  static QWidget* makeScriptableWidget(
      const Process::Port& port, const score::DocumentContext& ctx, QWidget* parent);

private:
  static QLabel* setupImpl(
      const QString& txt, const Port& port, const score::DocumentContext& ctx,
      Inspector::Layout& lay, QWidget* parent, bool withCables);
  static void setupControlImpl(
      const Process::Port& port, QWidget* inlet_widget,
      const score::DocumentContext& ctx, Inspector::Layout& lay, QWidget* parent);
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
      const Process::ProcessModel& proc, const score::DocumentContext& ctx,
      QWidget* parent);

  void reload();

private:
  const Process::ProcessModel& m_process;
  const score::DocumentContext& m_ctx;
  std::vector<QWidget*> m_inlets;
  std::vector<QWidget*> m_outlets;
};
}
