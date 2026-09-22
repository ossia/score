#pragma once
#include <score_plugin_scenario_export.h>

#include <verdigris>

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QListWidget;
class QSpinBox;
class QCheckBox;
class QJsonObject;
class QTimer;
namespace color_widgets
{
class ColorWheel;
}

namespace Scenario
{
namespace Settings
{
/**
 * @brief The skin editor, inlined as a settings sub-tab.
 *
 * Writes straight into score::Skin and emits changed(), so the UI updates as
 * you edit. Fonts are per-role: a global size cannot coexist with per-skin
 * fonts, since the skin's "fonts" block is applied over it and wins.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT SkinEditorWidget final : public QWidget
{
  W_OBJECT(SkinEditorWidget)
public:
  explicit SkinEditorWidget(QWidget* parent = nullptr);
  ~SkinEditorWidget() override;

  //! Select a skin in the list without emitting skinChanged().
  void setSkin(const QString& skin);

  //! In QSettings, so the model can honour them without a round trip through
  //! the settings machinery. Only governs what a newly picked skin
  //! contributes: what is in force afterwards is the saved state.
  static int selectedParts() noexcept;

  //! The colours and fonts actually in force, as the settings hold them. A
  //! skin file is only where they came from, and fills in whatever this does
  //! not name.
  static QJsonObject savedState() noexcept;

  //! Records the skin as it now stands.
  static void saveState() noexcept;

  void skinChanged(const QString& arg_1) W_SIGNAL(skinChanged, arg_1);

private:
  QWidget* makeSkinRow();
  QWidget* makeColorEditor();
  QWidget* makeFontEditor();

  //! Re-read the editor from the skin.
  void reloadFromSkin();
  //! Re-read the selected role into the font widgets.
  void loadFontRole();
  //! Write the font widgets back into the skin and refresh the UI.
  void applyFontRole();
  void refreshFontPreview();

  //! Coalesced: the wheel emits on every drag step, and the state is a few
  //! kilobytes of JSON.
  void scheduleSave();

  QComboBox* m_skin{};
  QCheckBox* m_applyColours{};
  QCheckBox* m_applyFonts{};

  QListWidget* m_colorList{};
  color_widgets::ColorWheel* m_wheel{};
  QLineEdit* m_hex{};

  QListWidget* m_fontList{};
  QComboBox* m_fontFamily{};
  QComboBox* m_fontStyle{};
  QSpinBox* m_fontSize{};
  QCheckBox* m_fontAntialias{};
  QComboBox* m_fontHinting{};
  QPlainTextEdit* m_fontPreview{};
  QLabel* m_fontHint{};

  //! Guards the widget -> skin direction while we are loading skin -> widget.
  bool m_loading{false};
  //! Set while this widget is the one changing the skin, so its own edits do
  //! not bounce back as a reload.
  bool m_applying{false};

  QTimer* m_saveTimer{};
};
}
}
