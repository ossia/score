#pragma once
#include <QPointer>
#include <QWidget>

#include <score_lib_process_export.h>

#include <verdigris>

class QStackedLayout;
class QCloseEvent;
namespace score
{
class DocumentDelegateView;
}

namespace Process
{
/**
 * @brief A code editor drawn over a live preview of what it edits.
 *
 * Central view for live coding: the document's background (a Background
 * device, a watched texture port) takes the whole area and the editor sits
 * on top of it without any background, so that the result of a compile is
 * visible without leaving the code.
 *
 * The host follows its two widgets: it closes when its editor goes away,
 * and shows the editor opaque again if the preview does.
 */
class SCORE_LIB_PROCESS_EXPORT EditorOverlayHost final : public QWidget
{
  W_OBJECT(EditorOverlayHost)
public:
  EditorOverlayHost(QWidget* editor, QWidget* preview, QWidget* parent = nullptr);
  ~EditorOverlayHost() override;

  QWidget* editor() const noexcept { return m_editor; }
  QWidget* preview() const noexcept { return m_preview; }

  //! Takes the editor out of the host, which then closes itself.
  QWidget* releaseEditor();

protected:
  void closeEvent(QCloseEvent* e) override;

private:
  QStackedLayout* m_layout{};
  QPointer<QWidget> m_editor;
  QPointer<QWidget> m_preview;
};

/**
 * @brief Paints what the document currently shows behind itself.
 *
 * Preview for a code editor drawn over the score's own background (a
 * Background device, a watched texture port): the same picture, so that the
 * editor shows the actual output rather than a private render of the process.
 */
class SCORE_LIB_PROCESS_EXPORT DocumentBackgroundPreview final : public QWidget
{
  W_OBJECT(DocumentBackgroundPreview)
public:
  explicit DocumentBackgroundPreview(
      score::DocumentDelegateView& view, QWidget* parent = nullptr);
  ~DocumentBackgroundPreview() override;

protected:
  void paintEvent(QPaintEvent*) override;
  void timerEvent(QTimerEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;

private:
  QPointer<score::DocumentDelegateView> m_view;
  int m_timer{-1};
};

//! Removes the background of an editor widget's text areas (false restores it).
SCORE_LIB_PROCESS_EXPORT
void setEditorTransparent(QWidget* editor, bool transparent);

//! Removes the chrome of an editor widget: frames, scroll bars, margins and
//! the button row (Ctrl+Return compiles). false restores it.
SCORE_LIB_PROCESS_EXPORT
void setEditorChromeless(QWidget* editor, bool chromeless);
}
