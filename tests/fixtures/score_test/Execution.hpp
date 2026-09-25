#pragma once

// A test document's execution, driven from the test thread.

#include <Execution/DocumentPlugin.hpp>

#include <ossia/detail/thread.hpp>

namespace score::test
{

//! Runs the pending execution commands as the audio thread, which they require.
inline void run_exec(Execution::DocumentPlugin& plug)
{
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
}

}
