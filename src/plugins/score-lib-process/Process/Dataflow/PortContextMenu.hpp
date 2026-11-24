#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <score_lib_process_export.h>

class QMenu;
class QObject;
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

/**
 * Adds entries to the menu a right click opens on the background of a node
 * view: the node view of a box, at the centre of the window, in a slot or in
 * a sub-scenario. `interval` is the box (a Scenario::IntervalModel) whose
 * processes the view shows.
 */
class SCORE_LIB_PROCESS_EXPORT NodalViewMenuFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(NodalViewMenuFactory, "5b2d8e41-9f3a-4c6e-8d17-2a4f6c9e0b35")
public:
  ~NodalViewMenuFactory() override;
  virtual void
  setupMenu(QMenu& menu, QObject& interval, const Process::Context& ctx) = 0;
};

class SCORE_LIB_PROCESS_EXPORT NodalViewMenuFactoryList final
    : public score::InterfaceList<NodalViewMenuFactory>
{
public:
  ~NodalViewMenuFactoryList() override;
};
}
