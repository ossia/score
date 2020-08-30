#pragma once
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <QDialog>

#include <score_lib_process_export.h>

class QPlainTextEdit;
class QCodeEditor;
class QTabWidget;
namespace Process
{
class ProcessModel;
class SCORE_LIB_PROCESS_EXPORT ScriptDialog : public QDialog
{
public:
  ScriptDialog(const score::DocumentContext& ctx, QWidget* parent);

  QSize sizeHint() const override { return {800, 300}; }
  QString text() const noexcept;

  void setText(const QString& str);
  void setError(int line, const QString& str);

protected:
  virtual void on_accepted() = 0;

  const score::DocumentContext& m_context;
  QCodeEditor* m_textedit{};
  QPlainTextEdit* m_error{};
};

template <typename Process_T, typename Property_T>
class ProcessScriptEditDialog : public ScriptDialog
{
public:
  ProcessScriptEditDialog(
      const Process_T& process,
      const score::DocumentContext& ctx,
      QWidget* parent)
    : ScriptDialog{ctx, parent}, m_process{process}
  {
    setText((m_process.*Property_T::get)());
    con(m_process, &Process_T::errorMessage, this, &ProcessScriptEditDialog::setError);
    con(m_process, &IdentifiedObjectAbstract::identified_object_destroying,
        this, &QWidget::deleteLater);
    con(m_process, Property_T::notify, this, &ProcessScriptEditDialog::setText);
  }

  void on_accepted() override
  {
    this->setError(0, QString{});
    if (this->text() != (m_process.*Property_T::get)())
    {
      // TODO try to see if we can make this a bit more efficient,
      // by passing the validated / transformed data to the command maybe ?
      if (m_process.validate(this->text()))
      {
        CommandDispatcher<>{m_context.commandStack}.submit(
              new score::StaticPropertyCommand<Property_T>{m_process, this->text()});
      }
    }
  }

protected:
  const Process_T& m_process;
};

}
