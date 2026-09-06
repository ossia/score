#pragma once
#include <QObject>
#include <QPointer>
#include <QString>

#include <score_lib_process_export.h>

#include <functional>

namespace Process
{
/**
 * @brief Where the UIs attached to a process get shown.
 *
 * Applies to the script / code editors (JS, shaders, DSP...) and to the
 * custom UIs of processes that can be embedded (JS ScriptUI components).
 * Plug-in windows that own a native window (VST, LV2, CLAP...) always
 * open as separate windows.
 */
enum class UIPlacement
{
  //! A separate top-level window.
  Window,
  //! A tab of the right pane, next to the inspector.
  SidePanel,
  //! A view of the central area, in place of the score, with the
  //! navigation bar staying on top.
  Central
};

//! How a script editor looks in the central view
enum class PreviewSource
{
  //! Without chrome nor background, over what the document shows behind
  //! itself (a Background device, a watched texture port) when there is one
  Background,
  //! A plain editor
  None
};

/**
 * The user's defaults, stored in the "User interface" settings.
 * The settings model lives in the scenario plug-in; the values are read
 * back from QSettings here so that this library does not depend on it.
 */
struct SCORE_LIB_PROCESS_EXPORT UIPlacementSettings
{
  static constexpr const char* scriptEditorKey = "Skin/ScriptEditorPlacement";
  static constexpr const char* processUIKey = "Skin/ProcessUIPlacement";
  static constexpr UIPlacement defaultScriptEditorPlacement = UIPlacement::SidePanel;
  static constexpr UIPlacement defaultProcessUIPlacement = UIPlacement::Central;

  //! How central script editors look
  static constexpr const char* scriptEditorPreviewKey = "Skin/ScriptEditorPreview";
  static constexpr PreviewSource defaultScriptEditorPreview = PreviewSource::Background;

  //! Identifiers stored in the settings, in the order of the UIPlacement enum.
  static const QStringList& names();
  static QString toString(UIPlacement p);
  static UIPlacement fromString(const QString& s, UIPlacement fallback);

  static UIPlacement scriptEditorPlacement();
  static UIPlacement processUIPlacement();

  //! Identifiers stored in the settings, in the order of the PreviewSource enum.
  static const QStringList& previewNames();
  static QString toString(PreviewSource p);
  static PreviewSource previewFromString(const QString& s, PreviewSource fallback);
  static PreviewSource scriptEditorPreview();
  static void setScriptEditorPreview(PreviewSource p);

  //! Changing the defaults from the UI; the listeners (the settings model)
  //! are told so that the settings panel follows.
  static void setScriptEditorPlacement(UIPlacement p);
  static void setProcessUIPlacement(UIPlacement p);
  //! The listener is dropped once its context object is gone.
  static void addChangeListener(QObject* context, std::function<void()> f);
};
}
