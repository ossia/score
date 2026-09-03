#include "ExpandableTextEdit.hpp"

#include <State/ValueConversion.hpp>

#include <QAction>
#include <QApplication>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <algorithm>

#include <wobjectimpl.h>

W_OBJECT_IMPL(State::ExpandableTextEdit)

namespace State
{
namespace
{
//! Painted rather than shipped as a resource, so it follows the palette.
QIcon ellipsisIcon(const QWidget& w, bool emphasized)
{
  const qreal dpr = w.devicePixelRatioF();
  const int side = std::max(12, w.fontMetrics().height());

  QPixmap px{QSize{side, side} * dpr};
  px.setDevicePixelRatio(dpr);
  px.fill(Qt::transparent);
  {
    QPainter p{&px};
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(
        w.palette().color(emphasized ? QPalette::Highlight : QPalette::WindowText));

    const qreal r = std::max(1.0, side / 12.0);
    const qreal y = side / 2.0;
    for(int i = -1; i <= 1; i++)
      p.drawEllipse(QPointF{side / 2.0 + i * (side / 4.0), y}, r, r);
  }
  return QIcon{px};
}

QFont monospace()
{
  QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  f.setPointSizeF(f.pointSizeF() - 0.5);
  return f;
}

QString toHex(const QByteArray& b)
{
  QString out;
  out.reserve(b.size() * 3);
  for(int i = 0; i < b.size(); i++)
  {
    if(i > 0)
      out += (i % 16 == 0) ? '\n' : ' ';
    out += QStringLiteral("%1").arg((unsigned char)b[i], 2, 16, QChar('0'));
  }
  return out;
}

QByteArray fromHex(const QString& s, bool& ok)
{
  QString digits;
  for(QChar c : s)
    if(!c.isSpace())
      digits += c;

  ok = (digits.size() % 2 == 0);
  if(!ok)
    return {};

  QByteArray out;
  out.reserve(digits.size() / 2);
  for(int i = 0; i < digits.size(); i += 2)
  {
    bool byteOk = false;
    const auto v = QStringView{digits}.mid(i, 2).toUShort(&byteOk, 16);
    if(!byteOk)
    {
      ok = false;
      return {};
    }
    out.append((char)v);
  }
  return out;
}

//! The right-hand column of a hex editor: one glyph per byte, dot for the
//! ones that have none.
QString toAscii(const QByteArray& b)
{
  QString out;
  out.reserve(b.size() + b.size() / 16);
  for(int i = 0; i < b.size(); i++)
  {
    if(i > 0 && i % 16 == 0)
      out += '\n';
    const auto u = (unsigned char)b[i];
    out += (u >= 0x20 && u < 0x7F) ? QChar(u) : QChar('.');
  }
  return out;
}

//! One byte per line of the offset gutter, in the usual 16-wide rows.
QString toOffsets(qsizetype size)
{
  QString out;
  for(qsizetype i = 0; i < std::max<qsizetype>(size, 1); i += 16)
  {
    if(i > 0)
      out += '\n';
    out += QStringLiteral("%1").arg(i, 6, 16, QChar('0'));
  }
  return out;
}

//! Bytes from a run of hex digits. An odd count is read as a leading half
//! byte, so that pasting "abc" gives 0a bc rather than dropping a digit.
QByteArray bytesFromDigits(const QString& digits)
{
  QByteArray out;
  out.reserve(digits.size() / 2 + 1);

  int i = 0;
  if(digits.size() % 2 == 1)
  {
    out.append((char)QStringView{digits}.left(1).toUShort(nullptr, 16));
    i = 1;
  }
  for(; i + 1 < digits.size(); i += 2)
    out.append((char)QStringView{digits}.mid(i, 2).toUShort(nullptr, 16));
  return out;
}
/**
 * @brief A column of a hex editor: a view of bytes that can only hold bytes.
 *
 * A keystroke changes one byte and the text is rewritten from the array; the
 * text is never parsed back, so there is no half-typed byte to report and the
 * character column cannot write its own dots over the data.
 *
 * Positions are in nibbles: the hex column needs half a byte, the character
 * column only lands on even ones. The caret may sit one past the last byte,
 * where typing appends.
 *
 *   Backspace  remove the byte before the caret, or the selected ones
 *   Delete     remove the byte at the caret, or the selected ones
 *   Insert     open a new byte at the caret
 *   Ctrl+Z/Y   undo and redo, in byte states rather than keystrokes
 */
class ByteColumn : public QPlainTextEdit
{
public:
  /**
   * @brief What Ctrl+Z walks back through.
   *
   * Shared by the two columns of one panel: an edit made in either is one step
   * for both, or undo would mean something different depending on which half
   * the cursor happens to be in.
   *
   * Capped by weight rather than by count -- a blob can be megabytes and each
   * state is a full copy of it.
   */
  struct History
  {
    struct State
    {
      QByteArray bytes;
      int nibble;
    };

    std::vector<State> states{State{{}, 0}};
    std::size_t at{};
    qsizetype held{};

    static constexpr qsizetype max_held = 8 * 1024 * 1024;

    void reset(const QByteArray& b)
    {
      states.assign(1, {b, 0});
      at = 0;
      held = b.size();
    }

    void push(const QByteArray& b, int nibble)
    {
      for(auto i = states.begin() + at + 1; i != states.end(); ++i)
        held -= i->bytes.size();
      states.resize(at + 1);

      states.push_back({b, nibble});
      held += b.size();

      while(states.size() > 1 && held > max_held)
      {
        held -= states.front().bytes.size();
        states.erase(states.begin());
      }
      at = states.size() - 1;
    }
  };

  //! Called whenever the user changes the bytes, never on setBytes().
  std::function<void(const QByteArray&)> onEdited;

  const QByteArray& bytes() const noexcept { return m_bytes; }

  //! The two columns of a panel share one; by default a column has its own.
  void setHistory(std::shared_ptr<History> h) { m_history = std::move(h); }

  //! A new value from outside: the history starts over on it.
  void setBytes(const QByteArray& b)
  {
    m_history->reset(b);
    showBytes(b);
  }

  //! The same value, edited in the sibling column: shown, not recorded, since
  //! the column that made the edit has already recorded it.
  void followBytes(const QByteArray& b)
  {
    if(b != m_bytes)
      showBytes(b);
  }

  //! An empty byte before the caret, for the panel's button as well as Ins.
  void insertByte()
  {
    m_nibble = nibbleOfPos(textCursor().position());
    m_bytes.insert(std::min<int>(m_nibble / 2, int(m_bytes.size())), '\0');
    edited(m_nibble - m_nibble % 2);
  }

  //! The byte at the caret, or every byte the selection touches.
  void removeBytes()
  {
    m_nibble = nibbleOfPos(textCursor().position());

    if(const auto [from, to] = selectedBytes(); to > from)
    {
      m_bytes.remove(from, to - from);
      edited(2 * from);
      return;
    }

    if(m_nibble / 2 < int(m_bytes.size()))
    {
      m_bytes.remove(m_nibble / 2, 1);
      edited(m_nibble - m_nibble % 2);
    }
  }

protected:
  explicit ByteColumn(QWidget* parent = nullptr)
      : QPlainTextEdit{parent}
  {
    setOverwriteMode(true);
    // The document is rewritten wholesale on every edit, so its own undo has
    // nothing useful in it; m_history holds byte states instead.
    setUndoRedoEnabled(false);
  }

  //! The whole column, as it should read for the bytes it holds.
  virtual QString render() const = 0;
  //! Where in that text a nibble starts, and which nibble a position is in.
  virtual int posOfNibble(int nib) const = 0;
  virtual int nibbleOfPos(int pos) const = 0;
  //! Take a typed character, or say it means nothing in this column.
  virtual bool type(QChar c) = 0;
  //! The bytes a paste of this text carries.
  virtual QByteArray pasted(const QString& text) const = 0;

  int lastNibble() const noexcept
  {
    return m_bytes.isEmpty() ? 0 : int(m_bytes.size()) * 2 - 1;
  }

  //! One past the last nibble: the caret sits here to append.
  int endNibble() const noexcept { return int(m_bytes.size()) * 2; }

  //! The byte a typed character lands on, appending one past the end.
  int openByte()
  {
    const int byte = m_nibble / 2;
    if(byte >= int(m_bytes.size()))
      m_bytes.append('\0');
    return byte;
  }

  void rewrite(int caretNibble)
  {
    m_nibble = std::clamp(caretNibble, 0, endNibble());

    const QSignalBlocker blocker{this};
    const int scroll = verticalScrollBar()->value();
    setPlainText(render());
    verticalScrollBar()->setValue(scroll);

    QTextCursor c = textCursor();
    c.setPosition(std::min(posOfNibble(m_nibble), document()->characterCount() - 1));
    setTextCursor(c);
  }

  //! The bytes without touching the history: a value from elsewhere.
  void showBytes(const QByteArray& b)
  {
    m_bytes = b;
    rewrite(std::min(m_nibble, endNibble()));
  }

  //! Apply an edit: redraw it, remember it, and tell the panel.
  void edited(int caretNibble)
  {
    rewrite(caretNibble);
    m_history->push(m_bytes, m_nibble);
    if(onEdited)
      onEdited(m_bytes);
  }

  QByteArray m_bytes;
  int m_nibble{};

private:
  void restore(std::size_t at)
  {
    m_history->at = at;
    m_bytes = m_history->states[at].bytes;
    rewrite(m_history->states[at].nibble);
    if(onEdited)
      onEdited(m_bytes);
  }

  //! The bytes the selection covers: every byte it touches at all.
  std::pair<int, int> selectedBytes() const
  {
    const QTextCursor c = textCursor();
    if(!c.hasSelection())
      return {0, 0};

    const int a = nibbleOfPos(c.selectionStart());
    const int b = nibbleOfPos(c.selectionEnd());
    return {
        std::clamp(a / 2, 0, int(m_bytes.size())),
        std::clamp((b + 1) / 2, 0, int(m_bytes.size()))};
  }

  void keyPressEvent(QKeyEvent* ev) override
  {
    if(ev->matches(QKeySequence::Undo))
    {
      if(m_history->at > 0)
        restore(m_history->at - 1);
      return;
    }
    if(ev->matches(QKeySequence::Redo))
    {
      if(m_history->at + 1 < m_history->states.size())
        restore(m_history->at + 1);
      return;
    }
    if(ev->matches(QKeySequence::Copy) || ev->matches(QKeySequence::SelectAll)
       || ev->matches(QKeySequence::Paste))
    {
      QPlainTextEdit::keyPressEvent(ev);
      return;
    }

    m_nibble = nibbleOfPos(textCursor().position());

    switch(ev->key())
    {
      case Qt::Key_Delete:
        removeBytes();
        return;

      case Qt::Key_Backspace:
      {
        if(const auto [from, to] = selectedBytes(); to > from)
        {
          m_bytes.remove(from, to - from);
          edited(2 * from);
          return;
        }

        // The byte before the caret, the way a keyboard is expected to; it is
        // also how an append is undone. From the low half of the first byte
        // there is none before it, so it takes that one -- doing nothing at
        // all reads as a stuck key.
        if(m_nibble >= 2)
        {
          m_bytes.remove(m_nibble / 2 - 1, 1);
          edited(m_nibble - m_nibble % 2 - 2);
        }
        else if(m_nibble == 1 && !m_bytes.isEmpty())
        {
          m_bytes.remove(0, 1);
          edited(0);
        }
        return;
      }

      case Qt::Key_Insert:
        insertByte();
        return;

      case Qt::Key_Left:
      case Qt::Key_Right:
      case Qt::Key_Up:
      case Qt::Key_Down:
      case Qt::Key_Home:
      case Qt::Key_End:
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        QPlainTextEdit::keyPressEvent(ev);
        m_nibble = nibbleOfPos(textCursor().position());
        return;

      default:
        break;
    }

    const QString t = ev->text();
    if(t.size() == 1 && type(t[0]))
      return;

    // Return, Tab, the function keys: nothing to do with bytes.
    ev->ignore();
  }

  void insertFromMimeData(const QMimeData* src) override
  {
    if(!src)
      return;

    const auto add = pasted(src->text());
    if(add.isEmpty())
      return;

    if(const auto [from, to] = selectedBytes(); to > from)
    {
      m_bytes.remove(from, to - from);
      m_nibble = 2 * from;
    }

    const int at = std::min<int>(m_nibble / 2, int(m_bytes.size()));
    m_bytes.insert(at, add);
    edited(2 * (at + int(add.size())));
  }

  void mouseReleaseEvent(QMouseEvent* ev) override
  {
    QPlainTextEdit::mouseReleaseEvent(ev);
    m_nibble = nibbleOfPos(textCursor().position());
  }

  std::shared_ptr<History> m_history{std::make_shared<History>()};
};

//! The hex half: two digits a byte, sixteen bytes a line. Space types a zero.
class HexEdit final : public ByteColumn
{
public:
  explicit HexEdit(QWidget* parent = nullptr)
      : ByteColumn{parent}
  {
  }

private:
  static int lineChars(int count) noexcept { return count > 0 ? 3 * count - 1 : 0; }

  int bytesOnLine(int line) const noexcept
  {
    return std::min<int>(16, int(m_bytes.size()) - 16 * line);
  }

  QString render() const override { return toHex(m_bytes); }

  //! Three characters a byte, a full line and its newline being 48 of them.
  static constexpr int line_chars = 16 * 3;

  int posOfNibble(int nib) const override
  {
    // Arithmetic rather than toHex().size(): this is on the keystroke path.
    const int n = int(m_bytes.size());
    const int byte = nib / 2, half = nib % 2;

    // Past the end: just after the last digit.
    if(byte >= n)
    {
      if(n == 0)
        return 0;
      const int last = (n - 1) / 16;
      return line_chars * last + lineChars(n - 16 * last);
    }

    return line_chars * (byte / 16) + 3 * (byte % 16) + half;
  }

  int nibbleOfPos(int pos) const override
  {
    // The very end of the text is the append spot, not the last byte.
    if(pos >= document()->characterCount() - 1)
      return endNibble();

    int line = 0, seen = 0;
    while(true)
    {
      const int cnt = bytesOnLine(line);
      const int len = lineChars(cnt);
      if(pos <= seen + len || cnt <= 0)
      {
        const int col = std::clamp(pos - seen, 0, std::max(0, len));
        const int idx = std::min(col / 3, std::max(0, cnt - 1));
        // Column 2 of a group is the separating space: it belongs to the byte
        // that follows it.
        const int hi = (col % 3 == 1) ? 1 : 0;
        const int byte = 16 * line + idx + (col % 3 == 2 ? 1 : 0);
        return std::clamp(2 * byte + hi, 0, lastNibble());
      }
      seen += len + 1;
      line++;
    }
  }

  bool type(QChar c) override
  {
    const char l = c.toLatin1();
    if(l == ' ')
      c = QChar('0');
    else if(!std::isxdigit((unsigned char)l))
      return false;

    const QString digit{c};
    const int v = QStringView{digit}.toUShort(nullptr, 16);
    const int byte = openByte();

    auto u = (unsigned char)m_bytes[byte];
    u = (m_nibble % 2 == 0) ? ((v << 4) | (u & 0x0F)) : ((u & 0xF0) | v);
    m_bytes[byte] = (char)u;

    edited(m_nibble + 1);
    return true;
  }

  //! Whatever hex digits the text carries, a lone trailing nibble included.
  QByteArray pasted(const QString& text) const override
  {
    QString digits;
    for(QChar c : text)
      if(std::isxdigit((unsigned char)c.toLatin1()))
        digits += c;
    return bytesFromDigits(digits);
  }
};

/**
 * @brief The character half: one glyph a byte, dot for the ones with none.
 *
 * Typing replaces the byte under the caret. The dots are never written back:
 * only a keystroke changes a byte.
 */
class AsciiEdit final : public ByteColumn
{
public:
  explicit AsciiEdit(QWidget* parent = nullptr)
      : ByteColumn{parent}
  {
  }

private:
  int bytesOnLine(int line) const noexcept
  {
    return std::min<int>(16, int(m_bytes.size()) - 16 * line);
  }

  QString render() const override { return toAscii(m_bytes); }

  //! One character a byte, a full line and its newline being 17 of them.
  static constexpr int line_chars = 16 + 1;

  int posOfNibble(int nib) const override
  {
    const int n = int(m_bytes.size());
    const int byte = nib / 2;

    if(byte >= n)
    {
      if(n == 0)
        return 0;
      const int last = (n - 1) / 16;
      return line_chars * last + (n - 16 * last);
    }

    return line_chars * (byte / 16) + byte % 16;
  }

  int nibbleOfPos(int pos) const override
  {
    if(pos >= document()->characterCount() - 1)
      return endNibble();

    int line = 0, seen = 0;
    while(true)
    {
      const int cnt = bytesOnLine(line);
      if(pos <= seen + cnt || cnt <= 0)
      {
        const int col = std::clamp(pos - seen, 0, std::max(0, cnt));
        return std::clamp(2 * (16 * line + col), 0, endNibble());
      }
      seen += cnt + 1;
      line++;
    }
  }

  bool type(QChar c) override
  {
    const auto l = (unsigned char)c.toLatin1();
    if(l < 0x20 || l >= 0x7F)
      return false;

    const int byte = openByte();
    m_bytes[byte] = (char)l;
    edited(2 * (byte + 1));
    return true;
  }

  //! The text itself, as bytes; what it cannot show is still stored.
  QByteArray pasted(const QString& text) const override { return text.toUtf8(); }
};
/**
 * @brief The full editor: a Qt::Popup panel, not a dialog.
 *
 * Clicking away commits, Escape cancels, Ctrl+Return commits in place.
 *
 * Two stacked views of one array of bytes, so each gets the whole panel:
 *
 *   Text  a plain editor.
 *   Hex   offset gutter, hex column, character column; the last two editable.
 *
 * All of them edit `m_bytes`; the pane an edit came from is not written back
 * to, so its caret stays put.
 */
class TextPopup final : public QFrame
{
public:
  using Done = std::function<void(std::optional<QByteArray>)>;

  TextPopup(
      const QString& subject, const QByteArray& bytes, bool binary, QWidget* anchor,
      Done done)
      : QFrame{anchor, Qt::Popup | Qt::FramelessWindowHint}
      , m_bytes{bytes}
      , m_done{std::move(done)}
  {
    setFrameShape(QFrame::StyledPanel);

    auto* outer = new QVBoxLayout{this};
    outer->setContentsMargins(6, 5, 6, 5);
    outer->setSpacing(5);

    // --- header: segmented switch, and who is being edited ---------------
    auto* head = new QHBoxLayout;
    head->setSpacing(0);

    m_asText = makeTab(tr("Text"));
    m_asHex = makeTab(tr("Hex"));
    auto* group = new QButtonGroup{this};
    group->addButton(m_asText, 0);
    group->addButton(m_asHex, 1);
    head->addWidget(m_asText);
    head->addWidget(m_asHex);
    head->addStretch(1);

    if(!subject.isEmpty())
    {
      auto* who = new QLabel{subject, this};
      who->setEnabled(false);
      head->addWidget(who);
    }
    outer->addLayout(head);

    // --- the two views, one panel's worth each ---------------------------
    m_stack = new QStackedWidget{this};

    m_text = new QPlainTextEdit;
    m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_text->setTabChangesFocus(true);
    m_stack->addWidget(m_text);

    m_stack->addWidget(buildHexPane());
    outer->addWidget(m_stack, 1);

    // --- footer: the byte count, and the actions with their keys ----------
    auto* foot = new QHBoxLayout;
    m_count = new QLabel{this};
    m_count->setEnabled(false);
    m_count->setFixedHeight(m_count->sizeHint().height());
    foot->addWidget(m_count);
    foot->addStretch(1);

    m_insert = footButton(tr("Insert"), tr("Insert a byte at the cursor (Ins)"));
    m_remove = footButton(tr("Remove"), tr("Remove the byte at the cursor (Del)"));
    auto* cancel = footButton(tr("Cancel"), tr("Leave the value as it was (Esc)"));
    auto* apply = footButton(tr("Done"), tr("Keep the edit (Ctrl+Return)"));

    for(auto* b : {m_insert, m_remove, cancel, apply})
      foot->addWidget(b);
    outer->addLayout(foot);

    connect(m_insert, &QToolButton::clicked, this, [this] {
      column()->insertByte();
      column()->setFocus(Qt::PopupFocusReason);
    });
    connect(m_remove, &QToolButton::clicked, this, [this] {
      column()->removeBytes();
      column()->setFocus(Qt::PopupFocusReason);
    });
    connect(cancel, &QToolButton::clicked, this, [this] {
      m_cancelled = true;
      close();
    });
    connect(apply, &QToolButton::clicked, this, [this] { close(); });

    connect(m_text, &QPlainTextEdit::textChanged, this, [this] {
      setBytes(m_text->toPlainText().toUtf8(), m_text);
    });

    connect(group, &QButtonGroup::idClicked, this, [this](int i) { setMode(i); });

    setBytes(bytes, nullptr);
    setMode(binary ? 1 : 0);
    resize(560, 320);
  }

  //! Empty when the user cancelled; the bytes are always whole otherwise.
  std::optional<QByteArray> result() const
  {
    if(m_cancelled)
      return std::nullopt;
    return m_bytes;
  }

  void focusEditor()
  {
    (m_stack->currentIndex() == 1 ? static_cast<QWidget*>(m_hex) : m_text)
        ->setFocus(Qt::PopupFocusReason);
  }

private:
  //! A footer action. Takes no focus: the caret stays where the user left it.
  QToolButton* footButton(const QString& label, const QString& tip)
  {
    auto* b = new QToolButton{this};
    b->setText(label);
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
  }

  //! The byte column a footer button acts on: the one being typed in.
  ByteColumn* column() const
  {
    return m_ascii->hasFocus() ? static_cast<ByteColumn*>(m_ascii)
                               : static_cast<ByteColumn*>(m_hex);
  }

  QToolButton* makeTab(const QString& label)
  {
    auto* b = new QToolButton{this};
    b->setText(label);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
  }

  QWidget* buildHexPane()
  {
    auto* pane = new QWidget;
    auto* lay = new QHBoxLayout{pane};
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);

    const QFont mono = monospace();
    const QFontMetrics fm{mono};

    auto dress = [&](QPlainTextEdit* e, int chars) {
      e->setFont(mono);
      e->setLineWrapMode(QPlainTextEdit::NoWrap);
      e->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      e->setTabChangesFocus(true);
      if(chars > 0)
        e->setFixedWidth(fm.horizontalAdvance(QString(chars, '0')) + 12);
      return e;
    };

    // Named: three monospace columns are otherwise indistinguishable to a
    // stylesheet or a test.
    m_offsets = dress(new QPlainTextEdit, 6);
    m_offsets->setObjectName("offsetColumn");
    m_offsets->setReadOnly(true);
    m_offsets->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_offsets->setEnabled(false);

    m_hex = static_cast<HexEdit*>(dress(new HexEdit, 0));
    m_hex->setObjectName("hexColumn");

    m_ascii = static_cast<AsciiEdit*>(dress(new AsciiEdit, 16));
    m_ascii->setObjectName("charColumn");
    m_ascii->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_hex->setHistory(m_history);
    m_ascii->setHistory(m_history);

    m_hex->onEdited = [this](const QByteArray& b) { setBytes(b, m_hex); };
    m_ascii->onEdited = [this](const QByteArray& b) { setBytes(b, m_ascii); };

    // Only the hex column carries a scrollbar; the other two follow it.
    for(auto* other : {(QPlainTextEdit*)m_offsets, (QPlainTextEdit*)m_ascii})
      connect(
          m_hex->verticalScrollBar(), &QScrollBar::valueChanged,
          other->verticalScrollBar(), &QScrollBar::setValue);

    lay->addWidget(m_offsets);
    lay->addWidget(m_hex, 1);
    lay->addWidget(m_ascii);
    return pane;
  }

  //! `from` is the pane the edit came from, and the one left untouched.
  void setBytes(QByteArray b, QWidget* from)
  {
    if(m_syncing)
      return;

    m_syncing = true;
    m_bytes = std::move(b);

    // The byte columns record their own edits; one made in the text pane has
    // to be recorded for them, or Ctrl+Z in hex would step over it.
    if(from == m_text)
      m_history->push(m_bytes, 0);

    if(from != m_text)
      m_text->setPlainText(QString::fromUtf8(m_bytes));

    // Bytes that are not text read back as replacement characters, and this
    // pane writes what it shows: one keystroke here would turn every one of
    // them into EF BF BD. The byte columns are the way to edit these.
    const bool binary = State::convert::isBinary(m_bytes);
    m_text->setReadOnly(binary);
    m_text->setToolTip(binary ? tr("Not text: edit the bytes in hex") : QString{});

    // followBytes, not setBytes: an edit in one column must not throw away
    // what the panel has to undo.
    if(from != m_hex)
      from ? m_hex->followBytes(m_bytes) : m_hex->setBytes(m_bytes);
    if(from != m_ascii)
      from ? m_ascii->followBytes(m_bytes) : m_ascii->setBytes(m_bytes);
    m_offsets->setPlainText(toOffsets(m_bytes.size()));

    m_syncing = false;
    updateCount();
  }

  void setMode(int i)
  {
    m_asText->setChecked(i == 0);
    m_asHex->setChecked(i == 1);
    m_stack->setCurrentIndex(i);

    // Inserting and removing a byte is a thing only the byte columns do.
    m_insert->setVisible(i == 1);
    m_remove->setVisible(i == 1);

    focusEditor();
    updateCount();
  }

  void updateCount()
  {
    // A line count means nothing about a blob.
    if(m_stack->currentIndex() == 1)
    {
      m_count->setText(tr("%1 bytes").arg(m_bytes.size()));
      return;
    }

    const int lines = m_text->document()->blockCount();
    m_count->setText(
        (lines > 1 ? tr("%1 lines, %2 bytes") : tr("%1 line, %2 bytes"))
            .arg(lines)
            .arg(m_bytes.size()));
  }

  void keyPressEvent(QKeyEvent* ev) override
  {
    if(ev->key() == Qt::Key_Escape)
    {
      m_cancelled = true;
      close();
      return;
    }
    // Return alone is a newline here; Ctrl+Return is "done".
    if((ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter)
       && (ev->modifiers() & Qt::ControlModifier))
    {
      close();
      return;
    }
    QFrame::keyPressEvent(ev);
  }

  // Clicking outside a Qt::Popup hides it rather than closing it, so both
  // paths report. The callback runs once; a nested event loop here would wedge
  // anything that opens the popup with no user present.
  void hideEvent(QHideEvent* ev) override
  {
    QFrame::hideEvent(ev);
    finish();
  }
  void closeEvent(QCloseEvent* ev) override
  {
    QFrame::closeEvent(ev);
    finish();
  }

  void finish()
  {
    if(std::exchange(m_finished, true))
      return;
    if(m_done)
      m_done(result());
    deleteLater();
  }

  QToolButton* m_asText{};
  QToolButton* m_asHex{};
  QStackedWidget* m_stack{};
  QPlainTextEdit* m_text{};
  QPlainTextEdit* m_offsets{};
  HexEdit* m_hex{};
  AsciiEdit* m_ascii{};
  std::shared_ptr<ByteColumn::History> m_history{std::make_shared<ByteColumn::History>()};
  QLabel* m_count{};
  QToolButton* m_insert{};
  QToolButton* m_remove{};
  QByteArray m_bytes;
  Done m_done;
  bool m_cancelled{};
  bool m_syncing{};
  bool m_finished{};
};
}

ExpandableTextEdit::ExpandableTextEdit(QWidget* parent)
    : QLineEdit{parent}
{
  m_expand = new QAction{this};
  m_expand->setToolTip(tr("Edit (Alt+Return)"));

  // A shortcut, not a keyPressEvent: shortcuts are matched before the widget
  // event filters, and QStyledItemDelegate answers a bare Return by closing
  // the editor this popup would open from.
  m_expand->setShortcut(QKeySequence{Qt::ALT | Qt::Key_Return});
  m_expand->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  connect(m_expand, &QAction::triggered, this, [this] { expand(); });
  addAction(m_expand, QLineEdit::TrailingPosition);

  connect(this, &QLineEdit::textEdited, this, [this](const QString& t) {
    if(!needsPopup())
      m_bytes = t.toUtf8();
  });

  refreshIcon();
}

ExpandableTextEdit::~ExpandableTextEdit() = default;

QByteArray ExpandableTextEdit::fullBytes() const noexcept
{
  return needsPopup() ? m_bytes : text().toUtf8();
}

QString ExpandableTextEdit::fullText() const noexcept
{
  return QString::fromUtf8(fullBytes());
}

void ExpandableTextEdit::setFullText(const QString& t)
{
  setFullBytes(t.toUtf8());
}

void ExpandableTextEdit::setFullBytes(const QByteArray& b)
{
  m_bytes = b;
  m_binary = State::convert::isBinary(b);
  m_multiline = !m_binary && State::convert::isMultiLine(QString::fromUtf8(b));
  refresh();
}

void ExpandableTextEdit::refresh()
{
  if(m_binary)
  {
    setReadOnly(true);
    const auto head = m_bytes.left(8).toHex(' ');
    QLineEdit::setText(
        tr("%1%2  [%3]")
            .arg(QString::fromLatin1(head))
            .arg(m_bytes.size() > 8 ? QStringLiteral("…") : QString{})
            .arg(tr("%n byte(s)", "", int(m_bytes.size()))));
    setToolTip(tr("Binary — edit as hex"));
  }
  else if(m_multiline)
  {
    // Read-only: a one-line field silently drops the rest on commit.
    setReadOnly(true);
    QLineEdit::setText(State::convert::toSingleLine(QString::fromUtf8(m_bytes)));
    setToolTip(QString::fromUtf8(m_bytes));
  }
  else
  {
    setReadOnly(false);
    QLineEdit::setText(QString::fromUtf8(m_bytes));
    setToolTip({});
  }
  refreshIcon();
}

void ExpandableTextEdit::setSubject(QString s)
{
  m_subject = std::move(s);
}

void ExpandableTextEdit::expandIfNeeded()
{
  // Once per editor. The delegates call this from setEditorData, which the
  // view runs again on every dataChanged for the row -- so on a live device
  // this would raise the panel again each time a value arrived, including
  // right after the user pressed Escape on it.
  if(std::exchange(m_offeredPopup, true))
    return;

  if(needsPopup())
    QTimer::singleShot(0, this, [this] { expand(); });
}

void ExpandableTextEdit::expand()
{
  if(m_popup)
  {
    m_popup->raise();
    return;
  }

  QPointer self{this};

  // Parented to this: QStyledItemDelegate closes an editor that loses focus,
  // unless the widget taking the focus is below it in the tree.
  auto* pop = new TextPopup{
      m_subject, fullBytes(), m_binary, this,
      [self](std::optional<QByteArray> res) {
    if(!self)
      return;

    self->m_popup = nullptr;
    if(!res || *res == self->m_bytes)
      return;

    self->setFullBytes(*res);
    self->fullTextEdited(self->fullText());
      }};

  m_popup = pop;
  pop->move(mapToGlobal(QPoint{0, height()}));
  pop->show();
  pop->focusEditor();
}

void ExpandableTextEdit::mouseDoubleClickEvent(QMouseEvent* ev)
{
  if(needsPopup())
  {
    ev->accept();
    expand();
    return;
  }
  QLineEdit::mouseDoubleClickEvent(ev);
}

void ExpandableTextEdit::changeEvent(QEvent* ev)
{
  QLineEdit::changeEvent(ev);
  if(ev->type() == QEvent::PaletteChange || ev->type() == QEvent::FontChange)
    refreshIcon();
}

void ExpandableTextEdit::refreshIcon()
{
  m_expand->setIcon(ellipsisIcon(*this, needsPopup()));
}
}
