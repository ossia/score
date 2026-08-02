# ossia score on Yocto / OpenEmbedded

`meta-ossia` builds ossia score into a bare-bones, flashable Linux image: kernel,
init, audio and GPU stacks, and score. No desktop environment. The goal is the
full score feature set on an appliance, not a small image.

## What is pinned

| | |
|---|---|
| openembedded-core | `yocto-5.3.4` (**whinlatter**) |
| bitbake | `yocto-5.3.4` |
| meta-openembedded | `whinlatter` |
| meta-qt6 | `6.12` → Qt **6.12.0** |
| Boost | **1.89.0** (from oe-core) |
| GCC | **15** |
| CMake | 4.1.2 |
| ffmpeg | 8.0 |
| C library | **glibc** |

There is no `poky/whinlatter` branch — poky stops at walnascar — so the stack is
openembedded-core plus bitbake directly, and the distro config lives here
(`meta-ossia/conf/distro/ossia.conf`) instead of coming from meta-poky.

Boost 1.89 falls inside the window libossia probes
(`3rdparty/libossia/cmake/deps/boost.cmake`, `find_package(Boost 1.N EXACT)` from
`BOOST_MINOR_LATEST` down to `BOOST_MINOR_MINIMAL`). That matters: outside it,
the CMake code falls back to downloading a Boost tarball at configure time,
which cannot work under BitBake's offline `do_configure`. If you move this layer
to a newer series, check that pairing first — wrynose is 1.90 (fine), oe-core
master is 1.91 (needs `BOOST_MINOR_LATEST` bumped in libossia).

`LAYERSERIES_COMPAT` also declares `wrynose` and `blacksail`; nothing in the
recipes depends on anything that changed between those series. blacksail
additionally needs meta-qt6 to declare compatibility, which it currently does
not.

## Building

Needs [kas](https://github.com/siemens/kas) (`pip install kas`) and the standard
Yocto host packages.

```bash
cd cmake/Deployment/Linux/Yocto
./build.sh                              # qemux86-64
./build.sh whinlatter-raspberrypi5      # Pi 5 / CM5, writes a .wic.bz2
OSSIA_YOCTO_TARGET=ossia-score-image-appliance ./build.sh
```

Budget several hours and 90–120 GB for the first build. `DL_DIR` and
`SSTATE_DIR` default to alongside the build directory so every machine shares
them; override them in `local.conf` to put them elsewhere. `rm_work` is on.

Bring `core-image-minimal` up on your target *before* adding score. Debugging a
fresh BitBake setup and a Qt cross-build at the same time is how these ports
stall.

### Running it

```bash
./run.sh                       # qemux86-64, boots and opens a viewer
OSSIA_QEMU_MEM=4096 ./run.sh   # more RAM
```

`run.sh` uses KVM and virgl, which together are worth about 6x on power-to-GUI,
and displays over VNC. Without `kvm` every guest instruction is interpreted and
the whole system is roughly 3x slower, which distorts any measurement.

then on the target:

```bash
ossia-score-launch cli --script /path/to/scene.js   # headless
ossia-score-launch eglfs                            # GPU, straight to DRM/KMS
ossia-score-launch vulkan                           # vkkhrdisplay, fastest
```

`ossia-score.service` is installed but not enabled; `systemctl enable
ossia-score` turns the image into a kiosk.

### No init system at all

For a fixed-function appliance that will never touch the network, score can be
PID 1:

```
init=/usr/bin/ossia-score-init
```

**Not `init=/usr/bin/ossia-score` directly** — that panics the kernel roughly
50 ms after handoff:

```
Kernel panic - not syncing: Attempted to kill init! exitcode=0x0000000b
```

Two separate reasons, both fatal on their own. score dies immediately in an
environment with no `/proc` and no `/sys` (Mesa enumerates DRM devices through
`/sys/class/drm`), and because it is PID 1 the SIGSEGV becomes a panic instead
of an exit. `ossia-score-init` is a ~30-line shell script that mounts what is
missing, sets `XDG_RUNTIME_DIR`/`XDG_CACHE_HOME`, and then runs score **as a
child in a restart loop rather than `exec`ing it** — so a crash restarts rather
than panicking, and PID 1's zombie reaping covers the VST/VST3/CLAP puppets and
Faust processes score forks. `CONFIG_DEVTMPFS_MOUNT=y` means `/dev`, including
`/dev/dri`, is already populated without udev, which is the one piece that would
otherwise be genuinely awkward.

Verified on qemux86-64: the full UI comes up, with no systemd, no udev, no
getty, no journal. Measured from qemu start, software rasteriser, KVM, 8 vCPU:

| | |
|---|---|
| kernel init done | 1.07s |
| score's first output | 1.20s |
| engine listening | 1.33s |
| GL context created | 1.37s |
| startup chatter ends | 1.53s |

Against the systemd path measured the same way — the two runs agree on kernel
init to within 11 ms, which is what makes the rest comparable:

| | `init=` | systemd |
|---|---|---|
| kernel init done | 1.066s | 1.077s |
| score's first output | **1.203s** | 2.371s |
| engine listening | **1.326s** | 2.508s |

So dropping init saves about **1.2s** of pre-score userspace. Treat that as an
upper bound: getting score's output onto the serial console under systemd needs
`systemd.journald.forward_to_console=1`, and echoing the journal to a UART is
itself not free, so some of the gap is the measurement. Note also that score's
own startup dominates either way — this is ~1.2s off a figure where score itself
accounts for several seconds.

What you give up is worth stating plainly: no hotplug (a USB MIDI interface or a
display connected after boot will not appear), no networking, no ssh, no logs.
That is a different set of trade-offs from the systemd image, not a strictly
better one — the systemd path reaches the same UI and keeps all of it.

One thing to fix before shipping this mode: score's startup update check stalls
for **~2.8s** on DNS when there is no network (`Host ossia.io not found`, then
the `addons.json` fetch). It does not block first paint, but it is pure waste on
an offline appliance. See the `StartScreen.hpp` note in the upstream list.

## Sizing and boot time

score's resident set is **~250 MB** once the UI is up, of which ~110 MB is
dirty; the rest is file-backed and evictable. Measured floors on qemux86-64:
**192 MB** for `ossia-score-image-appliance`, and more for `ossia-score-image`,
which also carries systemd. Below that score is OOM-killed during startup, where
it transiently allocates well above its steady state. **Budget 512 MB or more.**

Measured power-to-GUI on qemux86-64 with 2 GB, from power-on to the start screen
being painted:

| | KVM + virgl | KVM, software GL | TCG, software GL |
|---|---|---|---|
| kernel | 1.05s | 0.66s | 1.93s |
| score forked | 2.36s | 1.76s | 6.48s |
| score's own startup | **0.64s** | 4.3s | 11.9s |
| **start screen painted** | **≈3.0s** | 6.1s | 18.4s |
| `Startup finished` (systemd) | 2.64s | 1.84s | 7.26s |
| RSS | 206 MB | 312 MB | 312 MB |

The third column is what you get by accident, so it is worth being explicit: the
`kvm` argument and a GPU account for a 6x difference in power-to-GUI, and most
of what looks like "score is slow to start" is neither score nor this layer.

For the middle and left columns, qemu needs a virtio GPU. runqemu's
`egl-headless` option gives one:

```bash
runqemu kvm slirp snapshot egl-headless ossia-score-image qemuparams='-m 2048'
```

Nothing has to be rebuilt for this -- mesa enables the `virgl` Gallium driver
whenever `opengl` is in `DISTRO_FEATURES`, and qemu-system-native picks up
`virglrenderer` on the same condition. To confirm it actually engaged, check
that score logs no "Running on a software rasterizer" warning and that
`/sys/class/drm/` has a `renderD*` node (plain stdvga has none). Note that
`screendump` cannot capture under `egl-headless` -- there is no host surface --
so verify through `/sys/kernel/debug/dri/0/state` instead, where the scanout
framebuffer should show `allocated by = ossia-score`.

The service starts at `sysinit.target` with `DefaultDependencies=no`, ordered
only after `local-fs.target` and udev, so score forks 8 ms after the root
filesystem is up -- ahead of `basic.target` and `multi-user.target`. Networking,
avahi, bluetooth and sshd all come up during the several seconds score spends in
its own startup, which is time that would otherwise be spent waiting.

What is left is almost entirely score's own initialisation, and most of that is
software rasterisation: qemu has no GPU, so Qt falls back to LLVMpipe. On real
hardware with a working DRM driver that portion largely disappears. Enabling
virtio-gpu with virgl would remove it here too.

## What score itself needs

The distro drops `x11` from `DISTRO_FEATURES`, which means score is built with no
X11 or xcb headers at all. That requires the X11-optional changes in
`src/linuxcheck`, `score-plugin-gfx` and `score-plugin-vst3` (score PR #2170).
Without them, configure fails on `X11_X11_INCLUDE_PATH` being `NOTFOUND` before
anything is compiled. Add `x11` back to `DISTRO_FEATURES` if you are building an
older score.

`rapidfuzz-cpp` is packaged here (`recipes-support/`) because libossia calls
`find_package(rapidfuzz CONFIG REQUIRED)` under `OSSIA_USE_SYSTEM_LIBRARIES`,
which aborts before its own vendored fallback can run. libossia PR #919 fixes
that upstream; once it lands the recipe becomes optional rather than required.

## Feature coverage

Everything that cross-compiles is built — all plugins, all addons. Two
exclusions, both technical rather than size-driven, via `SCORE_DISABLED_PLUGINS`
in the recipe:

- **`score-addon-onnx`** — `FetchContent`s a *prebuilt x86_64* onnxruntime during
  configure. Needs the network, and the binaries do not cross-compile.
- **`score-addon-ultraleap`** — pulls a proprietary SDK at configure time.

`score-plugin-lv2`, `score-plugin-faust` and `score-plugin-ysfx` are built
against recipes in `meta-ossia/recipes-support` (lv2kit, faust, faustlibraries,
ysfx). They use `find_package(...)` + `if(NOT TARGET ...) return()`, so dropping
a PACKAGECONFIG silently drops the plugin rather than failing the build.

`score-plugin-jit` needs LLVM and Clang for the target, which is an hour of
build time and ~58 MB of packages; `PACKAGECONFIG:remove:pn-ossia-score = "jit"`
turns it off. Everything else — gfx, JS, media, VST3, CLAP, Pd, protocols, the
whole avnd plugin set and the remaining 18 addons — builds from the vendored
tree.

`SCORE_USE_SYSTEM_LIBRARIES=1` is set, but it is a *preference*: libossia's
`cmake/deps/*.cmake` fall back to the vendored copy when a package is missing, so
the many dependencies with no OE recipe (fmt, rapidfuzz, rubberband,
libsamplerate, zita, …) resolve in-tree. It is also what suppresses the
faustlibs `ExternalProject` git fetch, which would otherwise need the network
during `do_compile`.

## Performance

- `-O3` for score (`FULL_OPTIMIZATION` in the recipe); OE's default is `-O2`.
  Debug flags are left alone so `-dbg` packages still work.
- Release build, unity build, static plugins — one binary, no plugin `.so`
  indirection, no rpath lookups.
- LTO is available but **off** by default: score links a single ~70 MB static
  binary out of a unity build, and an LTO link at that size is very
  memory-hungry on the build host. `OSSIA_SCORE_LTO = "1"` enables it.
- Realtime scheduling is configured — `95-ossia-score-rt.conf` grants the
  `audio` group `rtprio 95` and unlimited `memlock`, and the systemd unit
  repeats those (units do not go through PAM). Without them the engine xruns
  under load.
- FFT goes through FFTW rather than KFR. That is what libossia selects for GCC
  builds (`3rdparty/libossia.cmake`), and it is deliberate — `.cninja/developer-gcc.cmake`
  turns KFR off for GCC.
- Per-machine CPU tuning is `DEFAULTTUNE`'s job. `qemux86-64` is generic; set it
  for real hardware.

## Source

The recipe builds the **release source archive**, not a git checkout — score has
39 submodules, libossia 44 more, and `ci/common.deps.sh` clones ~20 addon
repositories separately, so a `gitsm://` fetch of score alone would silently
produce a build with no addons. The archive from `ci/tarball.build.sh` already
contains all of it with the `.git` directories stripped, and is published on
every release.

`ossia-score_git.bb` is the default and builds, through `externalsrc`, whatever
checkout this layer ships inside. That is deliberate for development but it is
**not reproducible**: two people running the same command get whatever is in
their own tree. For a build anyone can reproduce, pin the release recipe:

```
PREFERRED_VERSION_ossia-score = "3.8.2"
```

To build a different working tree instead, in `local.conf`:

```
INHERIT += "externalsrc"
EXTERNALSRC:pn-ossia-score = "/path/to/score"
EXTERNALSRC_BUILD:pn-ossia-score = "/path/to/score-yocto-build"
```

Make sure submodules and addons are checked out (`git submodule update --init
--recursive` plus `ci/common.deps.sh LINUX`) — the recipe cannot do it for you,
since `do_fetch` is the only task allowed network access.

## Known rough edges

- `cmake/ScoreDeploymentLinux.cmake` runs `dpkg --print-architecture` and reads
  `/etc/lsb-release` at configure time to populate CPack variables. Under
  BitBake it reads the *host* distro. Harmless (nothing consumes those variables
  here) but it makes configure non-reproducible; worth guarding behind
  `NOT CMAKE_CROSSCOMPILING` upstream.
- `ossia_git_info.cmake` shells out to `git describe`; from a tarball it yields
  `GIT-NOTFOUND` in the version string.
- `inherit qt6-cmake` passes Qt's `-DINSTALL_*DIR` variables, which score does
  not consume. CMake will warn about unused variables. Harmless.
- The image is not size-tuned, by design. Measured on qemux86-64: the stripped
  `ossia-score` binary is **83 MB** (109 MB before stripping) since everything is
  statically linked into it, `.wic.xz` is **186 MB** and `.ext4.zst` **230 MB**,
  across 901 packages. The `common/qtfeatures` file in the `ossia/sdk` repo is a
  ready-made minimal Qt configuration if that ever matters.
