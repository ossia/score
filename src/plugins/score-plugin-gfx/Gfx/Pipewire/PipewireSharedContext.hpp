#pragma once

#include <libremidi/backends/linux/pipewire/context.hpp>

#include <QDebug>

#include <memory>

namespace Gfx::PipeWire
{

using SharedPipewireContext = std::shared_ptr<libremidi::pipewire::context>;

/** Acquire the process-wide PipeWire connection for a device that is about to
 *  create a pw_stream on it.
 *
 *  libremidi::pipewire::context::reconnect() destroys the pw_core, pw_context
 *  and pw_thread_loop the connection is built on and builds new ones. Every
 *  pw_stream another holder created on that core keeps pointing at the freed
 *  loop, so it may only be called while this is the sole strong reference.
 *  `who` names the caller in the diagnostic emitted when a reconnection is
 *  declined.
 *
 *  Returns an empty pointer when no usable connection could be obtained.
 */
inline SharedPipewireContext acquireSharedContext(const char* who) noexcept
{
  using connection_state = libremidi::pipewire::connection_state;

  auto ctx = libremidi::pipewire::shared_context();
  if(!ctx)
    return {};

  if(ctx->state() == connection_state::broken)
  {
    if(ctx.use_count() == 1)
    {
      ctx->reconnect();
    }
    else
    {
      qWarning() << who
                 << ": shared PipeWire connection is flagged broken but is "
                    "still held by"
                 << (ctx.use_count() - 1)
                 << "other client(s) with live streams on it; reusing it "
                    "instead of tearing it down";
    }
  }

  if(!ctx->ok() && !(ctx->pw_core_ptr() && ctx->thread_loop_handle()))
    return {};

  return ctx;
}

}
