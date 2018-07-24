#pragma once
#include <QWidget>

namespace score
{
struct DocumentContext;
}
namespace Process
{
class ProcessModel;
/**
 * @brief Show the list of ports / addresses
 *
 * For use in the process inspectors.
 */
class PortListWidget final
    : public QWidget
{
public:
  PortListWidget(
      const Process::ProcessModel& proc
      , const score::DocumentContext& ctx
      , QWidget* parent);

  void reload();

private:
  const Process::ProcessModel& m_process;
  const score::DocumentContext& m_ctx;
  std::vector<QWidget*> m_inlets;
  std::vector<QWidget*> m_outlets;
};
}
