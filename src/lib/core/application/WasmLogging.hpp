#pragma once

// Message output for the WebAssembly build.
//
// On wasm every log line written through stdio is an fprintf that reaches JS one
// character at a time (fiprintf -> vfiprintf -> __stdio_write -> _fd_write ->
// doWritev -> write -> put_char), and the browser captures a stack for each one.
// A runaway warning therefore does not merely fill the console, it makes DevTools
// unresponsive. This writes whole lines straight to console.* instead, and
// collapses consecutive identical messages syslog-style.

#include <QtGlobal>

class QMessageLogContext;
class QString;

namespace score::wasm
{
/**
 * @brief Write one message to the browser console, collapsing repeats.
 *
 * A fatal is never suppressed: it is the last thing that will ever be printed.
 */
void logMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg);

//! Whether the last message handled by logMessage() was dropped as a repeat.
bool lastMessageWasSuppressed() noexcept;
}
