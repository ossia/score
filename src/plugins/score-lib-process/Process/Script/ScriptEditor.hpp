#pragma once
#include <score/command/PropertyCommand.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <QDialog>

class QPlainTextEdit;
class QCodeEditor;
namespace Process
{
class ProcessModel;
class ScriptDialog
    : public QDialog
{
public:
  ScriptDialog(
      const score::DocumentContext& ctx,
      QWidget* parent);

  QSize sizeHint() const override { return {800, 300}; }
  QString text() const noexcept;

  void setText(const QString& str);
  void setError(const QString& str);

protected:
  virtual void on_accepted() = 0;

  const score::DocumentContext& m_context;
  QCodeEditor* m_textedit{};
  QPlainTextEdit* m_error{};
};

template<typename Process_T, typename Property_T>
class ProcessScriptEditDialog
    : public ScriptDialog
{
public:
  ProcessScriptEditDialog(
      const Process_T& process,
      const score::DocumentContext& ctx,
      QWidget* parent)
    : ScriptDialog{ctx, parent}
    , m_process{process}
  {
    setText(m_process.script());
    con(m_process, &Process_T::errorMessage,
        this, &ScriptDialog::setError);
    con(m_process, Property_T::notify,
        this, &ScriptDialog::setText);
  }

  void on_accepted() override
  {
    setError(QString{});
    if(this->text() != (m_process.*Property_T::get)())
    {
      CommandDispatcher<>{m_context.commandStack}.submit(
            new score::StaticPropertyCommand<Property_T>{m_process, this->text()}
            );
    }
  }

protected:
  const Process_T& m_process;
};

}

