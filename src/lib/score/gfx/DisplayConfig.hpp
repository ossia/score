#pragma once
#include <QString>
#include <QVector>

#include <score_lib_base_export.h>

namespace score::gfx
{
/**
 * @brief One physical output, as the machine reports it.
 *
 * `name` is the connector -- "HDMI-A-1", "DP-1" -- which is what a display is
 * actually identified by. Not an index into a screen list: that order changes
 * between machines and between boots, so a document that names one is a
 * document that lands on the wrong projector somewhere else.
 */
struct DisplayOutput
{
  QString name;
  bool connected{};
  //! Modes the connector reports, best first, as "1920x1080".
  QVector<QString> modes;
};

/**
 * @brief What the user asked of one output.
 *
 * `mode` follows the platform's own vocabulary: empty means leave it alone,
 * "off" and "skip" disable it, "preferred" and "current" defer to the display,
 * and anything else is "1920x1080" or "1920x1080@60".
 */
struct DisplayOutputSettings
{
  QString name;
  QString mode;
  QString format;
  bool primary{};
  //! Where this output sits in the arrangement, in pixels. Unset means the
  //! outputs are laid out left to right in order.
  int x{};
  int y{};
  bool hasPosition{};
  int physicalWidthMm{};
  int physicalHeightMm{};
  QString cloneOf;
};

//! Settings that belong to the machine rather than to one output.
struct SCORE_LIB_BASE_EXPORT DisplaySettings
{
  QString device;
  bool hardwareCursor{true};
  bool verticalLayout{};
  //! Render with no output at all, as "1920x1080". For a machine that computes
  //! frames for somebody else.
  QString headless;
  //! Degrees, applied by the platform to software-rendered content.
  int rotation{};
  bool hideCursor{};
  QVector<DisplayOutputSettings> outputs;

  /**
   * vkkhrdisplay addresses displays by index, not by name, and offers nothing
   * else: no JSON, no per-output layout, no cloning, and one screen at a time.
   * There is no honest way to derive these from the connector names above --
   * the order comes from the Vulkan driver -- so they are their own settings
   * rather than a translation. -1 means "leave it to the platform".
   */
  int vulkanPhysicalDeviceIndex{-1};
  int vulkanDisplayIndex{-1};
  int vulkanModeIndex{-1};

  bool isEmpty() const noexcept;
};

//! Which of the above a platform can actually honour. The settings UI asks
//! this rather than hiding things behind a platform-name comparison of its own.
struct DisplayCapabilities
{
  //! Connector names, modes, layout, cloning. eglfs through its JSON; Windows
  //! through ChangeDisplaySettingsEx; macOS through CGDisplayConfiguration.
  bool perOutputConfiguration{};

  //! vkkhrdisplay, which addresses a display by index and offers nothing else.
  bool indexedDisplaySelection{};

  /**
   * Whether the configuration only takes hold when the process starts again.
   *
   * True on the embedded platforms, where these are read once by the platform
   * plug-in and there is no way to revisit them. False where the system owns
   * the displays and can be asked to change them while running -- which also
   * means the change can be undone, so those platforms want a "keep this
   * setting?" confirmation that the embedded ones cannot offer.
   */
  bool requiresRestart{};

  //! Whether score may change the machine's display setup at all. On a desktop
  //! the answer is "only if asked": rearranging somebody's monitors because a
  //! score was opened would be hostile.
  bool appliesToSystemDisplays{};

  //! Whether anything here applies at all.
  bool anyConfiguration() const noexcept
  {
    return perOutputConfiguration || indexedDisplaySelection;
  }
};

//! What `platform` supports. Takes the name so that it can be asked about a
//! platform other than the running one -- an appliance is configured from a
//! desktop, where QGuiApplication::platformName() says "xcb".
SCORE_LIB_BASE_EXPORT DisplayCapabilities displayCapabilities(const QString& platform);

/**
 * @brief The outputs this machine has.
 *
 * Read from sysfs rather than from Qt: this has to answer before there is a
 * QGuiApplication to ask, and it must work while another process holds the
 * display. `drmRoot` exists so the walk can be pointed at a fixture.
 */
SCORE_LIB_BASE_EXPORT QVector<DisplayOutput>
enumerateOutputs(const QString& drmRoot = QStringLiteral("/sys/class/drm"));

/**
 * @brief `settings` as the JSON the KMS platform reads.
 *
 * Only what the user actually set is written: an absent key means "whatever
 * the driver decided", which is a better default than anything score could
 * invent.
 */
SCORE_LIB_BASE_EXPORT QByteArray toKmsConfig(const DisplaySettings& settings);

//! Where the configuration is kept. Not QSettings: this is read before there
//! is a QApplication to give QSettings its organisation name.
SCORE_LIB_BASE_EXPORT QString displayConfigPath();

SCORE_LIB_BASE_EXPORT DisplaySettings loadDisplaySettings(const QString& path);
SCORE_LIB_BASE_EXPORT bool
saveDisplaySettings(const DisplaySettings& settings, const QString& path);

/**
 * @brief Put the configuration into effect, if there is one.
 *
 * Every one of these is read once, when the platform plug-in starts, so this
 * has to run before QGuiApplication is constructed and does nothing useful
 * afterwards. Under eglfs it writes the KMS JSON next to the settings and
 * points the platform at it; under vkkhrdisplay it sets the three indices that
 * platform understands. A no-op where a window manager owns the display.
 */
SCORE_LIB_BASE_EXPORT void applyDisplayConfig();
}
