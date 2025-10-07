#include "PromptLineEdit.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFile>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextStream>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::PromptLineEdit)
namespace score
{
static constexpr int DEFAULT_HISTORY_MAX_LEN = 100;

PromptLineEdit::PromptLineEdit(QWidget* parent)
    : QLineEdit{parent}
    , m_historyMaxLen{DEFAULT_HISTORY_MAX_LEN}
    , m_historyIndex{-1}
    , m_hintBold{false}
{
  connect(this, &QLineEdit::textChanged, this, &PromptLineEdit::updateHint);
}

PromptLineEdit::~PromptLineEdit()
{
  if(m_completer)
  {
    disconnect(m_completer, nullptr, this, nullptr);
  }
}

void PromptLineEdit::historyAdd(const QString& line)
{
  if(line.isEmpty() || m_historyMaxLen == 0)
    return;

  // Don't add duplicates
  if(!m_history.isEmpty() && m_history.last() == line)
    return;

  // Add to history
  m_history.append(line);

  // Trim if exceeded max length
  while(m_history.size() > m_historyMaxLen)
    m_history.removeFirst();

  // Reset history index
  m_historyIndex = -1;
}

void PromptLineEdit::historySetMaxLen(int len)
{
  m_historyMaxLen = len;

  // Trim existing history if needed
  while(m_history.size() > m_historyMaxLen)
    m_history.removeFirst();
}

bool PromptLineEdit::historySave(const QString& filename)
{
  QFile file(filename);
  if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  QTextStream out(&file);
  for(const QString& line : std::as_const(m_history))
    out << line << "\n";

  return true;
}

bool PromptLineEdit::historyLoad(const QString& filename)
{
  QFile file(filename);
  if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  QTextStream in(&file);
  while(!in.atEnd())
  {
    QString line = in.readLine();
    historyAdd(line);
  }

  return true;
}

void PromptLineEdit::historyClear()
{
  m_history.clear();
  m_historyIndex = -1;
}

void PromptLineEdit::navigateHistory(bool up)
{
  if(m_history.isEmpty())
    return;

  // First time: stash current input
  if(m_historyIndex == -1)
  {
    m_historyStash = text();
    m_historyIndex = m_history.size();
  }

  // Navigate
  if(up)
  {
    if(m_historyIndex > 0)
      m_historyIndex--;
  }
  else
  {
    if(m_historyIndex < m_history.size())
      m_historyIndex++;
  }

  // Update text
  if(m_historyIndex < m_history.size())
  {
    setText(m_history[m_historyIndex]);
  }
  else
  {
    setText(m_historyStash);
    m_historyIndex = -1; // Back to no-history state
  }
}

void PromptLineEdit::setCompleter(QCompleter* completer)
{
  if(m_completer)
  {
    disconnect(m_completer, nullptr, this, nullptr);
  }

  m_completer = completer;

  if(!m_completer)
  {
    return;
  }

  m_completer->setWidget(this);
  m_completer->setCompletionMode(QCompleter::CompletionMode::PopupCompletion);

  connect(
      m_completer, QOverload<const QString&>::of(&QCompleter::activated), this,
      &PromptLineEdit::insertCompletion);
}

QCompleter* PromptLineEdit::completer() const
{
  return m_completer;
}

void PromptLineEdit::insertCompletion(const QString& s)
{
  if(m_completer->widget() != this)
  {
    return;
  }

  // Get the current text and cursor position
  QString currentText = text();
  int cursorPos = cursorPosition();

  // Find word boundaries - include dots for property/method completion
  int wordStart = cursorPos;
  while(wordStart > 0 && (currentText[wordStart - 1].isLetterOrNumber() || currentText[wordStart - 1] == '_' || currentText[wordStart - 1] == '.'))
  {
    wordStart--;
  }

  int wordEnd = cursorPos;
  while(wordEnd < currentText.length() && (currentText[wordEnd].isLetterOrNumber() || currentText[wordEnd] == '_' || currentText[wordEnd] == '.'))
  {
    wordEnd++;
  }

  // Replace the word under cursor with completion
  QString newText
      = currentText.left(wordStart) + s + currentText.mid(wordEnd);
  setText(newText);
  setCursorPosition(wordStart + s.length());
}

void PromptLineEdit::setHint(const QString& hint, const QColor& color, bool bold)
{
  m_hint = hint;
  m_hintColor = color.isValid() ? color : palette().color(QPalette::PlaceholderText);
  m_hintBold = bold;
  update(); // Trigger repaint
}

void PromptLineEdit::clearHint()
{
  m_hint.clear();
  update();
}

void PromptLineEdit::updateHint()
{
  this->hintRequested(text());
}

QString PromptLineEdit::wordUnderCursor() const
{
  QString currentText = text();
  int cursorPos = cursorPosition();

  // Find word boundaries - include dots for property/method completion
  int wordStart = cursorPos;
  while(wordStart > 0 && (currentText[wordStart - 1].isLetterOrNumber() || currentText[wordStart - 1] == '_' || currentText[wordStart - 1] == '.'))
  {
    wordStart--;
  }

  int wordEnd = cursorPos;
  while(wordEnd < currentText.length() && (currentText[wordEnd].isLetterOrNumber() || currentText[wordEnd] == '_' || currentText[wordEnd] == '.'))
  {
    wordEnd++;
  }

  return currentText.mid(wordStart, wordEnd - wordStart);
}

bool PromptLineEdit::proceedCompleterBegin(QKeyEvent* e)
{
  if(m_completer && m_completer->popup()->isVisible())
  {
    switch(e->key())
    {
      case Qt::Key_Enter:
      case Qt::Key_Return:
      case Qt::Key_Escape:
      case Qt::Key_Tab:
      case Qt::Key_Backtab:
        e->ignore();
        return true; // let the completer do default behavior
      default:
        break;
    }
  }

  // Ctrl+Space triggers completion
  auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);

  return !(!m_completer || !isShortcut);
}

void PromptLineEdit::proceedCompleterEnd(QKeyEvent* e)
{
  auto ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);

  if(!m_completer || (ctrlOrShift && e->text().isEmpty()) || e->key() == Qt::Key_Delete)
  {
    return;
  }

  // Characters that end a word (but dot is NOT included - it's used for property access)
  static QString eow(R"(~!@#$%^&*()_+{}|:"<>?,/;'[]\-= )");

  auto isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Space);
  auto completionPrefix = wordUnderCursor();

  // Show completion if:
  // - Manual trigger (Ctrl+Space), OR
  // - Text is not empty AND (prefix >= 2 chars OR just typed a dot)
  bool shouldShow = isShortcut || (!e->text().isEmpty() && (completionPrefix.length() >= 2 || e->text() == "."));

  if(!shouldShow || eow.contains(e->text().right(1)))
  {
    m_completer->popup()->hide();
    return;
  }

  if(completionPrefix != m_completer->completionPrefix())
  {
    m_completer->setCompletionPrefix(completionPrefix);
    m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
  }

  auto cursRect = cursorRect();
  cursRect.setWidth(
      m_completer->popup()->sizeHintForColumn(0)
      + m_completer->popup()->verticalScrollBar()->sizeHint().width());

  m_completer->complete(cursRect);
}

void PromptLineEdit::keyPressEvent(QKeyEvent* event)
{
  // Handle completer first
  auto completerSkip = proceedCompleterBegin(event);

  if(!completerSkip)
  {
    // Handle special keys
    switch(event->key())
    {
      case Qt::Key_Return:
      case Qt::Key_Enter: {
        QString line = text();
        if(!line.isEmpty())
        {
          historyAdd(line);
        }
        this->lineEntered(line);
        clear();
        m_historyIndex = -1;
        return;
      }

      case Qt::Key_Up:
        if(event->modifiers() == Qt::NoModifier)
        {
          navigateHistory(true);
          return;
        }
        break;

      case Qt::Key_Down:
        if(event->modifiers() == Qt::NoModifier)
        {
          navigateHistory(false);
          return;
        }
        break;

      case Qt::Key_L:
        if(event->modifiers() & Qt::ControlModifier)
        {
          clear();
          return;
        }
        break;

      case Qt::Key_U:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+U: Delete whole line
          clear();
          return;
        }
        break;

      case Qt::Key_K:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+K: Delete from cursor to end
          QString text = this->text();
          setText(text.left(cursorPosition()));
          return;
        }
        break;

      case Qt::Key_W:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+W: Delete previous word
          QString txt = text();
          int pos = cursorPosition();

          // Skip trailing spaces
          while(pos > 0 && txt[pos - 1].isSpace())
            pos--;

          // Delete word
          while(pos > 0 && !txt[pos - 1].isSpace())
            pos--;

          setText(txt.left(pos) + txt.mid(cursorPosition()));
          setCursorPosition(pos);
          return;
        }
        break;

      case Qt::Key_A:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+A: Move to start
          home(false);
          return;
        }
        break;

      case Qt::Key_E:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+E: Move to end
          end(false);
          return;
        }
        break;

      case Qt::Key_B:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+B: Move back one char
          cursorBackward(false, 1);
          return;
        }
        break;

      case Qt::Key_F:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+F: Move forward one char
          cursorForward(false, 1);
          return;
        }
        break;

      case Qt::Key_T:
        if(event->modifiers() & Qt::ControlModifier)
        {
          // Ctrl+T: Transpose characters
          QString txt = text();
          int pos = cursorPosition();
          if(pos > 0 && pos < txt.length())
          {
            QChar tmp = txt[pos - 1];
            txt[pos - 1] = txt[pos];
            txt[pos] = tmp;
            setText(txt);
            if(pos != txt.length() - 1)
              setCursorPosition(pos + 1);
          }
          return;
        }
        break;
    }

    // Default handling
    QLineEdit::keyPressEvent(event);
  }

  // Process completer after key event
  proceedCompleterEnd(event);
}

void PromptLineEdit::focusInEvent(QFocusEvent* e)
{
  if(m_completer)
  {
    m_completer->setWidget(this);
  }

  QLineEdit::focusInEvent(e);
}

void PromptLineEdit::paintEvent(QPaintEvent* event)
{
  QLineEdit::paintEvent(event);

  // Draw hint if present and text doesn't fill the line
  if(!m_hint.isEmpty() && !text().isEmpty())
  {
    QPainter painter(this);

    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(text());
    int hintX = textWidth + contentsMargins().left() + 5; // 5px spacing

    // Only draw if hint fits
    if(hintX < width() - contentsMargins().right())
    {
      QFont hintFont = font();
      if(m_hintBold)
        hintFont.setBold(true);
      painter.setFont(hintFont);
      painter.setPen(m_hintColor);

      // Calculate available width for hint
      int availableWidth = width() - hintX - contentsMargins().right();
      QString displayHint = fm.elidedText(m_hint, Qt::ElideRight, availableWidth);

      painter.drawText(hintX, height() / 2 + fm.ascent() / 2, displayHint);
    }
  }
}
}
