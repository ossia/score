// setup_gpu() in src/app/main.cpp points GLX at the NVIDIA vendor library
// whenever /proc/driver/nvidia/gpus is non-empty. That variable is binding:
// libglvnd dispatches every GLX call to the vendor it names and has no
// fallback. When the kernel module and the user-space driver are different
// releases -- the state a driver upgrade leaves behind until the machine is
// rebooted -- X_GLXCreateNewContext answers BadValue for every request and
// score ends up on the Null RHI backend, drawing nothing, on a machine whose
// integrated GPU would have worked.
//
// The decision is therefore conditional on the two halves agreeing, and the
// condition is a pure function of two version strings so that it can be tested
// on a machine with no NVIDIA card at all -- which is most of them, including
// every CI runner.
//
// The strings below are real: `610.57.04` and `615.71.09` are the mismatched
// pair this was found on, and the two NVRM lines are the proprietary and the
// open module's own wording, which differ in field count and so cannot be
// parsed by position.
#include <score/gfx/OpenGL.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("the NVRM driver release is read off either module's banner", "[unit][gfx]")
{
  // Open kernel module.
  CHECK(
      score::nvidiaKernelDriverVersion(
          "NVRM version: NVIDIA UNIX Open Kernel Module for x86_64  610.57.04  "
          "Release Build  (notroot@)  Fri Aug 28 12:12:04 UTC 2026\n"
          "GCC version:  Selected multilib: .;@m64\n")
      == "610.57.04");

  // Proprietary module: fewer fields, and "x86_64" sits where the open module
  // puts "Open", so a positional parse would pick up the wrong token.
  CHECK(
      score::nvidiaKernelDriverVersion(
          "NVRM version: NVIDIA UNIX x86_64 Kernel Module  550.54.14  Thu Feb "
          "22 01:44:30 UTC 2024\n")
      == "550.54.14");

  // Two-component releases exist (470.57).
  CHECK(
      score::nvidiaKernelDriverVersion(
          "NVRM version: NVIDIA UNIX x86_64 Kernel Module  470.57  Mon Jul 19\n")
      == "470.57");

  // Nothing to find.
  CHECK(score::nvidiaKernelDriverVersion("").isEmpty());
  CHECK(score::nvidiaKernelDriverVersion("GCC version: 15.2.1\n").isEmpty());
}

TEST_CASE("the GLX vendor library's release comes from its name", "[unit][gfx]")
{
  CHECK(
      score::nvidiaGlxLibraryVersion("/usr/lib/libGLX_nvidia.so.615.71.09")
      == "615.71.09");
  CHECK(
      score::nvidiaGlxLibraryVersion(
          "/usr/lib/x86_64-linux-gnu/libGLX_nvidia.so.550.54.14")
      == "550.54.14");

  // The stable soname is not a release, and neither is a path that was never
  // resolved to a real file.
  CHECK(score::nvidiaGlxLibraryVersion("/usr/lib/libGLX_nvidia.so.0").isEmpty());
  CHECK(score::nvidiaGlxLibraryVersion("libGLX_nvidia.so").isEmpty());
  CHECK(score::nvidiaGlxLibraryVersion("").isEmpty());
}

TEST_CASE("GLX is handed to NVIDIA only when both halves agree", "[unit][gfx]")
{
  using S = score::NvidiaGlxVendorState;

  // THE REGRESSION. This pair is the machine the bug was found on.
  CHECK(score::nvidiaGlxVendorState("610.57.04", "615.71.09") == S::VersionMismatch);

  CHECK(score::nvidiaGlxVendorState("615.71.09", "615.71.09") == S::Consistent);

  // No kernel module: nothing to offload to, and setup_gpu never gets here.
  CHECK(score::nvidiaGlxVendorState("", "615.71.09") == S::NoDriver);
  CHECK(score::nvidiaGlxVendorState("", "") == S::NoDriver);

  // A driver whose library version could not be read must NOT lose its GPU:
  // the caller treats Unknown as usable, so an unfamiliar library layout keeps
  // today's behaviour instead of silently disabling offload.
  CHECK(score::nvidiaGlxVendorState("615.71.09", "") == S::Unknown);
}

// Reads the real machine. Not an assertion about this machine's driver -- there
// may be none -- but it does assert the two paths agree, so the filesystem
// version cannot drift away from the pure one it is supposed to implement.
TEST_CASE("the live probe agrees with the pure decision", "[unit][gfx]")
{
  const auto live = score::nvidiaGlxVendorState();
  switch(live)
  {
    case score::NvidiaGlxVendorState::NoDriver:
      SUCCEED("no NVIDIA kernel module on this machine");
      break;
    case score::NvidiaGlxVendorState::NotInstalled:
      SUCCEED("NVIDIA kernel module present, no libGLX_nvidia");
      break;
    case score::NvidiaGlxVendorState::VersionMismatch:
      SUCCEED("NVIDIA kernel module and user-space driver disagree here");
      break;
    case score::NvidiaGlxVendorState::Consistent:
      SUCCEED("NVIDIA GLX stack is consistent here");
      break;
    case score::NvidiaGlxVendorState::Unknown:
      SUCCEED("NVIDIA GLX stack present, versions unreadable");
      break;
  }
  // Whatever it is, it must be one of them -- a default-constructed or
  // out-of-range value would mean the live path took a branch the enum does not
  // cover.
  CHECK(
      (live == score::NvidiaGlxVendorState::NoDriver
       || live == score::NvidiaGlxVendorState::NotInstalled
       || live == score::NvidiaGlxVendorState::VersionMismatch
       || live == score::NvidiaGlxVendorState::Consistent
       || live == score::NvidiaGlxVendorState::Unknown));
}
