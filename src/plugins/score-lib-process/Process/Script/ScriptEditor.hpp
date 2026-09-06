#pragma once

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <QCloseEvent>
#include <QDialog>

#include <score_lib_process_export.h>

#include <string_view>

class QPlainTextEdit;
class QTextEdit;
class QTabWidget;
namespace Process
{
class ProcessModel;
class SCORE_LIB_PROCESS_EXPORT ScriptDialog : public QDialog
{
public:
  ScriptDialog(
      const std::string_view lang, const score::DocumentContext& ctx, QWidget* parent);
  ~ScriptDialog();

  QSize sizeHint() const override { return {800, 300}; }
  QString text() const noexcept;

  void setText(const QString& str);
  void setError(int line, const QString& str);
  void openInExternalEditor(const QString& editorPath);
  void stopWatchingFile();

protected:
  virtual void on_accepted() = 0;

  void hideEvent(QHideEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

  const score::DocumentContext& m_context;
  QObject* m_compileFilter{};
  QTextEdit* m_textedit{};
  QPlainTextEdit* m_error{};

private:
  QString m_watchedFile;
  std::shared_ptr<std::function<void()>> m_fileHandle;
};

template <typename Process_T, typename Property_T, typename Spec_T>
class ProcessScriptEditDialog : public ScriptDialog
{
public:
  ProcessScriptEditDialog(
      const Process_T& process, const score::DocumentContext& ctx, QWidget* parent)
      : ScriptDialog{Spec_T::language, ctx, parent}
      , m_process{process}
  {
    setText((m_process.*Property_T::get)());
    con(m_process, &Process_T::errorMessage, this, &ProcessScriptEditDialog::setError);
    // The process goes first when its document closes: the dialog can still
    // get closed after that (with the document's view) and must not look at
    // it any more.
    con(m_process, &IdentifiedObjectAbstract::identified_object_destroying, this,
        [this] {
      m_processGone = true;
      deleteLater();
    });
    con(m_process, Property_T::notify, this, &ProcessScriptEditDialog::setText);
  }

  void on_accepted() override
  {
    this->setError(0, QString{});
    if(this->text() != (m_process.*Property_T::get)())
    {
      // TODO try to see if we can make this a bit more efficient,
      // by passing the validated / transformed data to the command maybe ?
      if(m_process.validate(this->text()))
      {
        CommandDispatcher<>{m_context.commandStack}.submit(
            new score::StaticPropertyCommand<Property_T>{
                m_process, this->text(), m_context});
      }
    }
  }

protected:
  const Process_T& m_process;
  bool m_processGone{};

  // The editor may be a window or be docked in the main window: in both
  // cases, going away means closing (which deletes it with WA_DeleteOnClose),
  // not merely hiding like QDialog::reject() would do on Escape.
  void reject() override { close(); }

  void closeEvent(QCloseEvent* event) override
  {
    if(!m_processGone && m_process.scriptUI == this)
    {
      const_cast<QWidget*&>(m_process.scriptUI) = nullptr;
      m_process.scriptUIVisible(false);
    }
    event->accept();
  }
};

}
