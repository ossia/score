#include "OngoingCommandManager.hpp"
#include <QDebug>


using namespace iscore;
// TODO this should be in the iscore namespace ?



// Old documentation. TODO Update it :
/**
 * These slots :
 *   * initiateOngoingCommand,
 *   * continueOngoingCommand,
 *   * validateOngoingCommand,
 *   * undoOngoingCommand
 *
 * Are to be used when a Command takes multiple "steps" that must be
 * checked by the user, and do impact the model.
 * For instance, when resizing an element with the mouse, it is necessary to see
 * the effects of the transformation on the whole model. But it has to be applied only once.
 *
 * The locking is for the network implementation : the specified object will appear "locked"
 * to other users and they won't be able to modify it, in order to prevent conflicts.
 *
 * First, call initiateOngoingCommand with the initial Command (for instance
 * in a mousePressEvent). (Command::mergeWith() must work).
 * Then, keep making new Commands at each "change" (for instance, each mouseMoveEvent) and
 * apply them with continueOngoingCommand.
 *
 * When everything is done (e.g. mouseReleaseEvent), validateOngoingCommand is to be called.
 * If the user wants to cancel his command, for instance by pressing the "Escape" key,
 * call undoOngoingCommand()
 *
 * A signal will be sent in case of validateOngoingCommand, to propagate it to the network.
 *
 */
/*
void initiateOngoingCommand(iscore::SerializableCommand*, QObject* objectToLock);
void continueOngoingCommand(iscore::SerializableCommand*);
void rollbackOngoingCommand();
void validateOngoingCommand();*/

// NOTE : the main problem here is the creation commands
// We should maybe juste use commandQueue()->push / pushAndEmit
// for move commands
// and use this one only for creation commands.
