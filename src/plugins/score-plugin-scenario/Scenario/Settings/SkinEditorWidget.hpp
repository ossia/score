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
namespace color_widgets
{
class ColorWheel;
}

namespace Scenario
{
namespace Settings
{
/**
 * @brief The skin editor, inlined as a settings sub-tab rather than a dialog.
 *
 * Picks the skin, and edits the live one: colours on the left list, fonts on
 * the right. Everything writes straight into score::Skin and emits changed(),
 * so the whole UI updates as you edit, and Save writes Skin::toJson(), which
 * carries both the colours and the fonts.
 *
 * Fonts are per-role here on purpose. A global font size cannot coexist with
 * per-skin fonts, because the skin's "fonts" block is applied over it and
 * therefore wins; the roles are also what actually lets you pick a different
 * size for a title than for a port label.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT SkinEditorWidget final : public QWidget
{
  W_OBJECT(SkinEditorWidget)
public:
  explicit SkinEditorWidget(QWidget* parent = nullptr);

  //! Select a skin in the list without emitting skinChanged().
  void setSkin(const QString& skin);

  //! Which halves of a skin the user wants applied when switching. Stored in
  //! QSettings so the model can honour them without a round trip through the
  //! settings machinery.
  static int selectedParts() noexcept;

  void skinChanged(const QString& arg_1) W_SIGNAL(skinChanged, arg_1);

private:
  QWidget* makeSkinRow();
  QWidget* makeColorEditor();
  QWidget* makeFontEditor();

  //! Re-read the whole editor from the skin, after a different one loads.
  void reloadFromSkin();
  //! Re-read the selected role into the font widgets.
  void loadFontRole();
  //! Write the font widgets back into the skin and refresh the UI.
  void applyFontRole();
  void refreshFontPreview();

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
  //! Set while this widget is the one changing the skin, so that its own
  //! edits do not bounce back through Skin::changed as a full reload.
  bool m_applying{false};
};
}
}
