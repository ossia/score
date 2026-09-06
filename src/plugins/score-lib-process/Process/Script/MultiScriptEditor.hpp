#pragma once
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <QCloseEvent>
#include <QDialog>

#include <score_lib_process_export.h>

class QPlainTextEdit;
class QTextEdit;
class QTabWidget;

namespace Process
{
class ProcessModel;
class SCORE_LIB_PROCESS_EXPORT MultiScriptDialog : public QDialog
{
public:
  MultiScriptDialog(const score::DocumentContext& ctx, QWidget* parent);

  QSize sizeHint() const override { return {800, 300}; }
  std::vector<QString> text() const noexcept;

  void addTab(const QString& name, const QString& text, const std::string_view language);
  void setText(int idx, const QString& str);
  void setError(const QString& str);
  void clearError();
  void openInExternalEditor(const QString& editorPath);

protected:
  virtual void on_accepted() = 0;

  void hideEvent(QHideEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

  const score::DocumentContext& m_context;
  QObject* m_compileFilter{};
  QTabWidget* m_tabs{};
  struct EditorTab
  {
    QTextEdit* textedit{};
  };
  std::vector<EditorTab> m_editors;
  QPlainTextEdit* m_error{};
};

template <typename Process_T, typename Property_T>
class ProcessMultiScriptEditDialog : public MultiScriptDialog
{
public:
  using param_type = typename Property_T::param_type;
  ProcessMultiScriptEditDialog(
      const Process_T& process, const score::DocumentContext& ctx, QWidget* parent)
      : MultiScriptDialog{ctx, parent}
      , m_process{process}
  {
    const auto& prop = (m_process.*Property_T::get)();
    for(auto& [name, addr, lang] : param_type::specification)
    {
      addTab(name, prop.*addr, lang);
    }

    con(m_process, Property_T::notify, this, [this]() {
      const auto& prop = (m_process.*Property_T::get)();
      int i = 0;
      for(auto& [name, addr, lang] : param_type::specification)
      {
        setText(i, prop.*addr);
        i++;
      }
    });

    con(m_process, &Process_T::errorMessage, this,
        &ProcessMultiScriptEditDialog::setError);
    // The process goes first when its document closes: the dialog can still
    // get closed after that (with the document's view) and must not look at
    // it any more.
    con(m_process, &IdentifiedObjectAbstract::identified_object_destroying, this,
        [this] {
      m_processGone = true;
      deleteLater();
    });
  }

  void on_accepted() override
  {
    this->clearError();
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

  // See ProcessScriptEditDialog
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
