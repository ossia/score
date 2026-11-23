#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <score_lib_process_export.h>

class QMenu;
namespace Process
{
class Port;
struct Context;

/**
 * Adds entries to the menu a right click on a port opens, in the timeline and
 * in the node view: what a plug-in can do with the port (record it, ...),
 * after what the port itself offers.
 */
class SCORE_LIB_PROCESS_EXPORT PortContextMenuFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(PortContextMenuFactory, "a8f3c2d1-6b47-4e9a-9c15-3e7d0b2f8a64")
public:
  ~PortContextMenuFactory() override;
  virtual void
  setupMenu(QMenu& menu, const Process::Port& port, const Process::Context& ctx) = 0;
};

class SCORE_LIB_PROCESS_EXPORT PortContextMenuFactoryList final
    : public score::InterfaceList<PortContextMenuFactory>
{
public:
  ~PortContextMenuFactoryList() override;
};
}
