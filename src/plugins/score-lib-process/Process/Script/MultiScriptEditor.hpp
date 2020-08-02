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
class SCORE_LIB_PROCESS_EXPORT MultiScriptDialog : public QDialog
{
public:
  MultiScriptDialog(const score::DocumentContext& ctx, QWidget* parent);

  QSize sizeHint() const override { return {800, 300}; }
  QString text() const noexcept;

  void addTab(const QString& name, const QString& text);
  void setText(int idx, const QString& str);
  void setError(int idx, int line, const QString& str);

protected:
  virtual void on_accepted() = 0;

  const score::DocumentContext& m_context;
  QTabWidget* m_tabs{};
  struct EditorTab {
    QCodeEditor* textedit{};
    QPlainTextEdit* error{};
  };
  std::vector<EditorTab> m_editors;
};

template <typename Process_T, typename Property_T>
class ProcessMultiScriptEditDialog : public MultiScriptDialog
{
public:
  using param_type = typename Property_T::param_type;
  ProcessMultiScriptEditDialog(
      const Process_T& process,
      const score::DocumentContext& ctx,
      QWidget* parent)
    : MultiScriptDialog{ctx, parent}, m_process{process}
  {
    const auto& prop = (m_process.*Property_T::get)();
    for(auto& [name, addr] : param_type::specification)
    {
      addTab(name, prop.*addr);
    }
    /*
    setText((m_process.*Property_T::get)());
    con(m_process, &Process_T::errorMessage, this, &ProcessMultiScriptEditDialog::setError);
    con(m_process, &IdentifiedObjectAbstract::identified_object_destroying,
        this, &QWidget::deleteLater);
    con(m_process, Property_T::notify, this, &ProcessMultiScriptEditDialog::setText);
    */
  }


  void on_accepted() override
  {
    /*
    this->setError(0, QString{});
    if (this->text() != (m_process.*Property_T::get)())
    {
      CommandDispatcher<>{m_context.commandStack}.submit(
            new score::StaticPropertyCommand<Property_T>{m_process, this->text()});
    }
    */
  }


protected:
  const Process_T& m_process;
};

}
