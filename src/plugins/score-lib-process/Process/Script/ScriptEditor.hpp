#pragma once

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

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

  const score::DocumentContext& m_context;
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
    con(m_process, &IdentifiedObjectAbstract::identified_object_destroying, this,
        &QWidget::deleteLater);
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
  void closeEvent(QCloseEvent* event) override
  {
    const_cast<QWidget*&>(m_process.externalUI) = nullptr;
    m_process.externalUIVisible(false);
  }
};

}
