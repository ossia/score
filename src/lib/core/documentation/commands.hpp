#pragma once
/*! \page Commands Commands
 *
 * Commands are used for undo-redo.
 * A command is simply a class which inherits from score::Command.
 *
 * Commands can be serialized: this allows to restore everything on a crash
 * (see score::DocumentBackupManager).
 *
 * \section CreatingCommand Creating a command
 *
 * * Inherit from score::Command
 * * Add the required data members that will allow to perform undo and redo.
 *   These should be most of the time:
 *    * The Path to the model object that will be changed by the command.
 *    * The old value and the new value
 * * Add a relevant constructor. Most of the time it will take a reference to
 * the changed object, and the new value, since the old one can be queried from
 * this object.
 * * Reimplement score::Command::undo() and score::Command::redo()
 * * Reimplement the serialization methods.
 * * Add the SCORE_COMMAND_DECL macro with the relevant metadata for the
 * command. The use of this macro is mandatory: the build system scans the code
 * and generate files that will automatically register all the commands. This
 * way, when there is a crash, commands can be deserialized correctly. They can
 * also be sent through the network this way. This also means that if a command
 * is added, CMake may need to be re-run to register the command and make it
 * available to the crash restoring.
 *
 *
 * Some examples of commands that can be used as base :
 * * Scenario::Command::SetCommentText for a very simple command that only
 * changes a text
 * * Scenario::Command::AddOnlyProcessToInterval for a command that leverages
 * interfaces and creates new elements in a score
 *
 *
 * \section LaunchingCommands Launching commands
 *
 * Launching a command requires access to the command stack (see
 * score::CommandStack). A reference to the current document's command stack is
 * available in `score::DocumentContext`.
 *
 * In the simplest case, sending a command would look like :
 *
 * \code
 * CommandDispatcher<> dispatcher{ctx.commandStack};
 * dispatcher.submit<TheCommand>(commandArg1, commandArg1, ...);
 * \endcode
 *
 * See for instance Scenario::EventPresenter : a dispatcher is stored as a
 * class member. On a drag'n'drop, a command is sent.
 *
 * \section SpecialCommands Special commands
 *
 * To simplify some common use cases, the following simplfified command classes
 * are available :
 *
 * * score::PropertyCommand : commands which just change a value of a member
 *                             within the Qt Property System
 * (http://doc.qt.io/qt-5/properties.html).
 *
 * * score::AggregateCommand : used when a command is made of multiple small
 * commands one after each other. For instance, the
 * Scenario::Commands::ClearSelection command is an aggregate of various
 * commands that clear various kinds of elements respectively. It should be
 * subclassed to give a meaningful name to the user.
 *
 * \subsection Dispatchers Command dispatchers
 *
 * Sometimes just sending a command is not good enough.
 * For instance, when moving a slider, we don't want to send a command each
 * time the value changes, even though it is altered in the model.
 *
 * Hence, some commands will have an additional `update()` method that takes
 * the same argument as the constructor of the command.
 *
 * The dispatchers are able to selectively create a new command the first time,
 * and update it when `submit` is called again.
 * The command is then pushed in the command stack with `commit()`, or
 * abandoned with `rollback()`.
 *
 * The various useful dispatchers are :
 *
 * * MacroCommandDispatcher : appends commands to an AggregateCommand.
 * * SingleOngoingCommandDispatcher : performs the updating mechanism mentioned
 * above on a single command enforced at compile time.
 * * MultiOngoingCommandDispatcher : performs the updating mechanism on
 * multiple following commands. That is, the following code :
 *
 * \code
 * MultiOngoingCommandDispatcher disp{stack};
 * disp.submit<Command1>(obj1, 0);
 * disp.submit<Command1>(obj1, 1);
 * disp.submit<Command1>(obj1, 2);
 * disp.submit<Command2>(obj2, obj3, obj4);
 * disp.submit<Command3>(obj1, 10);
 * disp.submit<Command3>(obj1, 20);
 * disp.commit<MacroCommand>();
 * \endcode
 *
 * Will behind the scene do the following :
 * \code
 * auto cmd = new Command1(obj1, 0);
 * cmd->redo();
 * cmd->update(obj1, 1);
 * cmd->redo();
 * cmd->update(obj1, 2);
 * cmd->redo();
 *
 * auto cmd2 = new Command2(obj2, obj3, obj4);
 * cmd2->redo();
 *
 * auto cmd3 = new Command3(obj1, 10);
 * cmd3->redo();
 * cmd3->(obj1, 20);
 * cmd3->redo();
 *
 * auto macro = new MacroCommand{cmd, cmd2, cmd3};
 * commandStack->push(macro);
 * \endcode
 *
 */
