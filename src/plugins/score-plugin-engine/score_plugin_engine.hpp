#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <vector>
#include <verdigris>

/**
 * \namespace Engine
 * \brief Link of score with the OSSIA API execution engine.
 *
 * This namespace provides the tools that are used when going from
 * the score model, with purely serializable data structures, to
 * the OSSIA API model.
 *
 * OSSIA implementations of the protocolsare also provided.
 *
 * There are three main parts :
 *
 * * \ref LocalTree handles the conversion from score's data
 * structures to
 * the Local protocol.
 * * \ref Execution handles the conversion from score's data
 * structures to
 * the OSSIA classes responsible for the execution (ossia::time_process,
 * ossia::clock, etc.)
 * * \ref Engine::Network wraps the various OSSIA protocols (Minuit, OSC, etc.)
 *   behind Device::DeviceInterface, and provides edition widgets for these
 * protocols.
 * * Classes used to handle the various node listening strategies are provided.
 *
 * Two files, \ref score2OSSIA.hpp and \ref OSSIA2score.hpp contain tools
 * to convert the various data structures of each environment into each other.
 */

/**
 * \namespace LocalTree
 * \brief Local tree provides a way to extend the tree given through the \ref
 * Engine::Network::LocalDevice.
 *
 * It is a device tree used to access to score's internal data structures
 * from the outside,
 * or use it in automations, etc.
 *
 * For instance, it allows to do conditions based on the advancement of each
 * Scenario::IntervalModel.
 *
 * It is possible to extend the tree for Process::ProcessModel%s, or other
 * polymorphic types.
 */

/**
 * \namespace Execution
 * \brief Components used for the execution of a score.
 *
 * This hierarchy of types is used to create the OSSIA structures for the
 * execution behind
 * score::Component%s.
 * <br>
 * Currently, all the execution structures are recreated from scratch when
 * doing "play".
 * <br>
 * The classes inheriting from Execution::ProcessComponent
 * are first created, then the Execution::IntervalComponent will ask
 * them for their ossia::time_process which it gives to the matching
 * ossia::time_interval.
 *
 * \section LiveModification Live modification during execution.
 * The execution engine allows live modification of scores.
 * Since the execution happens in a different thread than edition, we have to
 * be extremely careful however. <br> Instead of locking all the data
 * structures of the OSSIA API with mutex, which may slow down the execution,
 * we instead have a lock-free queue of edition commands. <br> Modifications
 * are submitted from the component hierarchy : <br>
 * * Execution::ScenarioComponent
 * * Execution::IntervalComponent
 * * etc...
 * <br><br>
 * To the OSSIA structures :
 * <br>
 * * ossia::time_process
 * * ossia::time_interval
 * * etc...
 * <br>
 * <br>
 *
 * A modification follows this pattern :
 * \code
 * [ User modification in the GUI ]
 *          |
 *          v
 * [ Commands applied ]
 *          |
 *          v
 * [ Models modified and
 *   modification signals emitted ]
 *          |
 *          v
 * [ Execution components
 *   catches the signal ]
 *          |
 *          v
 * [ Command inserted into
 *   Execution::Context::executionQueue ]
 *          |
 *          v
 * [ The execution algorithm applies the
 *   commands at the end of the current tick ]
 * \endcode
 *
 * For modification of values, for instance the duration of a
 * Interval, this is easily visible.
 * See for instance Execution::IntervalComponentBase 's constructor.
 * <br>
 * For creation and removal of objects, this should be handled automatically by
 * the various ComponentHierarchy classes which take care of creating and
 * removing the objects in the correct order. The Component classes just have
 * to provide functions that will do the actual instantiation, and pre- & post-
 * removal steps. <br><br> The actual "root" execution algorithm is given in
 * Execution::DefaulClock::makeDefaultCallback
 *
 * \subsection ExecutionThreadSafety Execution Thread Safety
 * One must take care when modifying the Execution classes, since thins
 * happen on two different threads.
 *
 * The biggest problem is that the score structures could be created and
 * deleted in a single tick. For instance when doing a complete undo - redo of
 * the whole undo stack. <br> This means that anything send to the command
 * queue must absolutely never access any of the score structures (for instance
 * Scenario::IntervalModel, etc) directly : they have to be copied. Else, there
 * *will* be crashes, someday. <br> In the flow graph shown before, everything
 * up to and including "Command inserted into the execution queue" happens in
 * the GUI thread, hence one can rely on everything "being here" at this point.
 * However, in the actual commands, the only things safe to use are :
 * * Copies of data : simple values, ints, etc, are safe.
 * * Shared pointers : unlike most other places in score, the Execution
 * components are not owned by their parents, but through shared pointers. This
 * means that shall the component be removed, if the pointer was copied in the
 * ExecutionCommand, there is no risk of crash. But one must take care of
 * copying the actual `shared_ptr` and not just the `this` pointer for
 * instance. Multiple classes inherit from `std::enable_shared_from_this` to
 * allow a `shared_from_this()` function that gives back a `shared_ptr` to the
 * this instance.
 *
 */

/**
 * \namespace Engine::Network
 * \brief OSSIA protocols wrapped into score
 *
 * This namespace provides the implementations in score for
 * various protocols: OSC, MIDI, Minuit, HTTP, WebSocket...
 */

class score_plugin_engine final : public score::ApplicationPlugin_QtInterface,
                                  public score::FactoryList_QtInterface,
                                  public score::FactoryInterface_QtInterface,
                                  public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "d4758f8d-64ac-41b4-8aaf-1cbd6f3feb91")
public:
  score_plugin_engine();
  virtual ~score_plugin_engine();

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::vector<std::unique_ptr<score::InterfaceListBase>> factoryFamilies() override;

  // Contains the OSC, MIDI, Minuit factories
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
