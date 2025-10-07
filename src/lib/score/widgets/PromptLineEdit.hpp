#pragma once
#include <QColor>
#include <QLineEdit>
#include <QString>
#include <QStringList>

#include <score_lib_base_export.h>

#include <verdigris>

class QKeyEvent;
class QPaintEvent;
class QCompleter;

namespace score
{
/**
 * @brief PromptLineEdit - A QLineEdit with readline-like features
 *
 * This widget provides:
 * - Command history navigation (Up/Down arrows)
 * - Advanced popup-based QCompleter completion
 * - History persistence
 */
class SCORE_LIB_BASE_EXPORT PromptLineEdit : public QLineEdit
{
  W_OBJECT(PromptLineEdit)

public:
  explicit PromptLineEdit(QWidget* parent = nullptr);
  ~PromptLineEdit() override;

  void historyAdd(const QString& line);
  bool historySave(const QString& filename);
  bool historyLoad(const QString& filename);
  void historySetMaxLen(int len);
  void historyClear();
  QStringList historyGetAll() const { return m_history; }

  void lineEntered(const QString& line)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, lineEntered, line);

  void setCompleter(QCompleter* completer);
  QCompleter* completer() const;

  void setHint(const QString& hint, const QColor& color = QColor(), bool bold = false);
  void clearHint();
  void hintRequested(const QString& currentText)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, hintRequested, currentText);

public:
  void insertCompletion(const QString& s);
  W_SLOT(insertCompletion);

private:
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void focusInEvent(QFocusEvent* e) override;

  // Helper methods
  void navigateHistory(bool up);
  void updateHint();

  // Completer processing
  bool proceedCompleterBegin(QKeyEvent* e);
  void proceedCompleterEnd(QKeyEvent* e);
  QString wordUnderCursor() const;

  // History management
  QStringList m_history;
  int m_historyMaxLen{};
  int m_historyIndex{};
  QString m_historyStash; // Stash current line when browsing history

  // QCompleter-based completion
  QCompleter* m_completer{};

  // Hints
  QString m_hint;
  QColor m_hintColor{};
  bool m_hintBold{};
};
}
