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
./build.sh whinlatter-raspberrypi4-64   # Pi 4 / CM4, writes a .wic.bz2
```

Budget several hours and 90–120 GB for the first build. Set `DL_DIR` and
`SSTATE_DIR` (or `OSSIA_YOCTO_WORK_DIR`) somewhere persistent before you start —
otherwise every rebuild starts from zero. `rm_work` is on by default, which keeps
the work directories from running to tens of GB.

Bring `core-image-minimal` up on your target *before* adding score. Debugging a
fresh BitBake setup and a Qt cross-build at the same time is how these ports
stall.

### Running it

```bash
kas shell kas/whinlatter-qemux86-64.yml -c "runqemu nographic ossia-score-image"
```

then on the target:

```bash
ossia-score-launch cli --script /path/to/scene.js   # headless
ossia-score-launch eglfs                            # GPU, straight to DRM/KMS
ossia-score-launch vulkan                           # vkkhrdisplay, fastest
```

`ossia-score.service` is installed but not enabled; `systemctl enable
ossia-score` turns the image into a kiosk.

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

Three plugins will self-disable because no OE recipe exists for their
dependency. They use `find_package(...)` + `if(NOT TARGET ...) return()`, so the
build succeeds and simply lacks them:

| plugin | needs | upstream |
|---|---|---|
| `score-plugin-lv2` | lilv, suil, lv2 | https://gitlab.com/lv2 (meson) |
| `score-plugin-faust` | libfaust + faustlibraries | https://github.com/grame-cncm/faust — pin `730eff6d` for the libs, as `score-plugin-faust/CMakeLists.txt` does |
| `score-plugin-ysfx` | ysfx | https://github.com/jpcima/ysfx |

Writing those four recipes is the remaining work for genuinely complete feature
parity. Everything else — gfx, JS, media, VST3, CLAP, Pd, protocols, the whole
avnd plugin set, and the remaining 18 addons — builds from the vendored tree.

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
