#pragma once
#include <QByteArray>
#include <QLineEdit>
#include <QPointer>

#include <score_lib_state_export.h>

#include <verdigris>

class QAction;

namespace State
{
/**
 * @brief A one-line field with a way out to a full editor.
 *
 * The value is held as bytes -- ossia's STRING is a std::string, and QLineEdit
 * drops newlines and non-printable characters. The field takes three shapes:
 *
 *  - one line of text: an ordinary editable line edit;
 *  - several lines: a read-only `first line  [+2 lines]` summary, whole text
 *    in the tooltip, popup to edit;
 *  - bytes that are not text: a read-only hex preview, popup on its hex pane.
 *
 * The popup is a Qt::Popup panel: clicking away commits, Escape cancels.
 *
 * fullBytes() / setFullBytes() are the real accessors; the QString pair is a
 * UTF-8 façade over them.
 */
class SCORE_LIB_STATE_EXPORT ExpandableTextEdit final : public QLineEdit
{
  W_OBJECT(ExpandableTextEdit)
public:
  explicit ExpandableTextEdit(QWidget* parent = nullptr);
  ~ExpandableTextEdit();

  QByteArray fullBytes() const noexcept;
  void setFullBytes(const QByteArray& b);

  QString fullText() const noexcept;
  void setFullText(const QString& t);

  //! Named in the popup's header, e.g. the address being edited.
  void setSubject(QString s);

  bool isMultiLine() const noexcept { return m_multiline; }
  bool isBinary() const noexcept { return m_binary; }

  //! The one-line field cannot carry this value: the popup is the only way in.
  bool needsPopup() const noexcept { return m_multiline || m_binary; }

  //! Opens the popup under the field.
  void expand();

  //! expand() when needsPopup(); the delegates call it after loading.
  void expandIfNeeded();

public:
  //! The popup committed a new value.
  void fullTextEdited(const QString& arg_1)
      E_SIGNAL(SCORE_LIB_STATE_EXPORT, fullTextEdited, arg_1)

private:
  void mouseDoubleClickEvent(QMouseEvent* ev) override;
  void changeEvent(QEvent* ev) override;

  void refresh();
  void refreshIcon();

  QAction* m_expand{};
  QPointer<QWidget> m_popup;
  QByteArray m_bytes;
  QString m_subject;
  bool m_multiline{};
  bool m_binary{};
  bool m_offeredPopup{};
};
}
