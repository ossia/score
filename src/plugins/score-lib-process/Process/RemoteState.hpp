#pragma once
#include <score_lib_process_export.h>

#include <QPointer>

#include <vector>

namespace Process
{
class ProcessModel;

/**
 * @brief Objects a command created here whose real content is on another
 * machine.
 *
 * A command carries what a factory would be *given*, not what the object would
 * *write*, and what it is given can describe a world this machine is not in. A
 * library entry for a shader carries the path of the file it was scanned from:
 * the factory may well exist here and still produce an empty process, because
 * the file is over there. A missing factory is only the loudest case of the
 * same thing.
 *
 * So this is not "stand-ins": it is everything whose state has to come from the
 * peer that issued the command. Whoever replicated the command is responsible
 * for asking; this is how it finds them without walking the document after
 * every edit, which at a control's update rate is not affordable.
 */
SCORE_LIB_PROCESS_EXPORT std::vector<QPointer<ProcessModel>>&
awaitingRemoteState() noexcept;
}
