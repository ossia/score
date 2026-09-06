#include "ScriptEditor.hpp"

#include "MultiScriptEditor.hpp"
#include "ScriptTabBar.hpp"
#include "ScriptWidget.hpp"

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/FileWatch.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QCodeEditor>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Process
{
// Ctrl+Return compiles. The code editor emits livecodeTrigger for it, but
// only when the modifiers are exactly Control: the numeric keypad's Enter
// carries KeypadModifier. So the key is also caught here, on the editors
// themselves, before anything else looks at it. (With the completion popup
// open the key goes to the popup first and completes instead.)
class CompileKeyFilter final : public QObject
{
public:
  CompileKeyFilter(QObject* parent, std::function<void()> compile)
      : QObject{parent}
      , m_compile{std::move(compile)}
  {
  }

  static bool isCompileKey(QKeyEvent* ke) noexcept
  {
    const bool enter = ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter;
    const auto mods = ke->modifiers() & ~Qt::KeypadModifier;
    return enter && mods == Qt::ControlModifier;
  }

  bool eventFilter(QObject* obj, QEvent* ev) override
  {
    switch(ev->type())
    {
      case QEvent::ShortcutOverride:
      case QEvent::KeyPress: {
        auto ke = static_cast<QKeyEvent*>(ev);
        if(!isCompileKey(ke))
          return false;
        ke->accept();
        if(ev->type() == QEvent::KeyPress)
          m_compile();
        return true;
      }
      default:
        return false;
    }
  }

private:
  std::function<void()> m_compile;
};

static QObject* addCompileShortcuts(QDialog* dialog, std::function<void()> compile)
{
  auto filter = new CompileKeyFilter{dialog, std::move(compile)};
  dialog->installEventFilter(filter);
  for(auto w : dialog->findChildren<QWidget*>())
    w->installEventFilter(filter);
  return filter;
}

ScriptDialog::ScriptDialog(
    const std::string_view language, const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_context{ctx}
{
  this->resize(800, 800);
  auto lay = new QVBoxLayout{this};
  this->setLayout(lay);

  m_textedit = createScriptWidget(language);

  m_error = new QPlainTextEdit{this};
  m_error->setReadOnly(true);
  m_error->setMaximumHeight(120);
  m_error->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

  lay->addWidget(m_textedit);
  lay->addWidget(m_error);
  lay->setStretch(0, 3);
  lay->setStretch(1, 1);
  auto bbox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Reset | QDialogButtonBox::Close, this};
  if(auto editorPath = QSettings{}.value("Skin/DefaultEditor").toString();
     !editorPath.isEmpty())
  {
    auto openExternalBtn = new QPushButton{tr("Edit in default editor"), this};
    connect(openExternalBtn, &QPushButton::clicked, this, [this, editorPath] {
      openInExternalEditor(editorPath);
    });
    bbox->addButton(openExternalBtn, QDialogButtonBox::HelpRole);
    openExternalBtn->setToolTip(
        tr("Edit in the default editor set in Score Settings > User Interface"));
    auto icon = makeIcons(
        QStringLiteral(":/icons/undock_on.png"),
        QStringLiteral(":/icons/undock_off.png"),
        QStringLiteral(":/icons/undock_off.png"));
    openExternalBtn->setIcon(icon);
  }
  bbox->button(QDialogButtonBox::Ok)->setText(tr("Compile"));
  bbox->button(QDialogButtonBox::Reset)->setText(tr("Clear log"));
  connect(bbox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [this] {
    m_error->clear();
  });
  lay->addWidget(bbox);

  auto ce = qobject_cast<QCodeEditor*>(m_textedit);
  connect(ce, &QCodeEditor::livecodeTrigger, this, &ScriptDialog::on_accepted);
  m_compileFilter = addCompileShortcuts(this, [this] { on_accepted(); });
  connect(bbox, &QDialogButtonBox::accepted, this, &ScriptDialog::on_accepted);
  connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ScriptDialog::~ScriptDialog()
{
  stopWatchingFile();
}

// Escape closes a QDialog, which is fine for an editor in its own window.
// Docked in the main window it would tear the editor down in the middle of
// a set from a key that also dismisses the completion popup and the search
// bar (they handle it themselves before it gets here).
static bool swallowEscape(const QDialog& dialog, QKeyEvent* event)
{
  if(event->key() == Qt::Key_Escape && !dialog.isWindow())
  {
    event->accept();
    return true;
  }
  return false;
}

void ScriptDialog::keyPressEvent(QKeyEvent* event)
{
  if(swallowEscape(*this, event))
    return;
  QDialog::keyPressEvent(event);
}

void MultiScriptDialog::keyPressEvent(QKeyEvent* event)
{
  if(swallowEscape(*this, event))
    return;
  QDialog::keyPressEvent(event);
}

void ScriptDialog::hideEvent(QHideEvent* event)
{
#if defined(__EMSCRIPTEN__)
  // Return keyboard/text focus to the main window: on wasm each window owns its
  // own hidden text-input element and focus follows the active window, so
  // without this the main window's text fields stay dead after closing here.
  if(auto mw = score::GUIAppContext().mainWindow)
  {
    mw->activateWindow();
    mw->raise();
  }
#endif
  QDialog::hideEvent(event);
}

QString ScriptDialog::text() const noexcept
{
  return m_textedit->document()->toPlainText();
}

void ScriptDialog::setText(const QString& str)
{
  if(str != text())
  {
    m_textedit->setPlainText(str);
  }
}

void ScriptDialog::setError(int line, const QString& str)
{
  m_error->setPlainText(str);
}

void ScriptDialog::openInExternalEditor(const QString& editorPath)
{
#if QT_CONFIG(process)
  if(editorPath.isEmpty())
  {
    QMessageBox::warning(
        this, tr("Error"), tr("no 'Default editor' configured in score settings"));
    return;
  }

  auto& w = score::FileWatch::instance();

  m_watchedFile = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                  + "/ossia_script_temp.js";
  if(m_fileHandle)
  {
    // we have to unwatch the previous file first
    w.remove(m_watchedFile, m_fileHandle);
    m_fileHandle.reset();
  }

  QFile file(m_watchedFile);
  if(!file.open(QIODevice::WriteOnly))
  {
    QMessageBox::warning(this, tr("Error"), tr("failed to create temporary file."));
    m_watchedFile.clear();
    return;
  }

  file.write(this->text().toUtf8());

  m_fileHandle = std::make_shared<std::function<void()>>([this]() {
    QFile file(m_watchedFile);
    if(file.open(QIODevice::ReadOnly))
    {
      QMetaObject::invokeMethod(
          m_textedit, [this, str = QString::fromUtf8(file.readAll())] {
        if(m_textedit)
        {
          m_textedit->setPlainText(str);
          on_accepted();
        }
      });
    }
  });

  if(!QProcess::startDetached(editorPath, QStringList{m_watchedFile}))
  {
    QMessageBox::warning(this, tr("Error"), tr("failed to launch external editor"));
    m_watchedFile.clear();
    m_fileHandle.reset();
  }
  else
  {
    w.add(m_watchedFile, m_fileHandle);
  }
#endif
}

void ScriptDialog::stopWatchingFile()
{
  if(m_watchedFile.isEmpty())
    return;

  auto& w = score::FileWatch::instance();
  w.remove(m_watchedFile, m_fileHandle);
  m_watchedFile.clear();
  m_fileHandle.reset();
}

MultiScriptDialog::MultiScriptDialog(const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_context{ctx}
{
  this->resize(800, 800);
  auto lay = new QVBoxLayout{this};
  this->setLayout(lay);

  m_tabs = new ScriptTabWidget;
  lay->addWidget(m_tabs);

  m_error = new QPlainTextEdit;
  m_error->setReadOnly(true);
  m_error->setMaximumHeight(120);
  m_error->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

  lay->addWidget(m_error);

  auto bbox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Reset | QDialogButtonBox::Close, this};

  lay->addWidget(bbox);

  bbox->button(QDialogButtonBox::Ok)->setText(tr("Compile"));
  bbox->button(QDialogButtonBox::Reset)->setText(tr("Clear log"));
  connect(bbox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [this] {
    m_error->clear();
  });

  m_compileFilter = addCompileShortcuts(this, [this] { on_accepted(); });
  connect(bbox, &QDialogButtonBox::accepted, this, &MultiScriptDialog::on_accepted);
  connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

  if(auto editorPath = QSettings{}.value("Skin/DefaultEditor").toString();
     !editorPath.isEmpty())
  {
    auto openExternalBtn = new QPushButton{tr("Edit in default editor"), this};
    connect(openExternalBtn, &QPushButton::clicked, this, [this, editorPath] {
      openInExternalEditor(editorPath);
    });
    bbox->addButton(openExternalBtn, QDialogButtonBox::HelpRole);
    openExternalBtn->setToolTip(
        tr("Edit in the default editor set in Score Settings > User Interface"));
    auto icon = makeIcons(
        QStringLiteral(":/icons/undock_on.png"),
        QStringLiteral(":/icons/undock_off.png"),
        QStringLiteral(":/icons/undock_off.png"));
    openExternalBtn->setIcon(icon);
  }
}

void MultiScriptDialog::addTab(
    const QString& name, const QString& text, const std::string_view language)
{
  auto textedit = createScriptWidget(language);
  textedit->setText(text);
  auto ce = qobject_cast<QCodeEditor*>(textedit);
  connect(ce, &QCodeEditor::livecodeTrigger, this, &MultiScriptDialog::on_accepted);

  m_tabs->addTab(textedit, name);
  m_editors.push_back({textedit});
  if(m_compileFilter)
    textedit->installEventFilter(m_compileFilter);
}

void MultiScriptDialog::hideEvent(QHideEvent* event)
{
#if defined(__EMSCRIPTEN__)
  if(auto mw = score::GUIAppContext().mainWindow)
  {
    mw->activateWindow();
    mw->raise();
  }
#endif
  QDialog::hideEvent(event);
}

std::vector<QString> MultiScriptDialog::text() const noexcept
{
  std::vector<QString> vec;
  vec.reserve(m_editors.size());
  for(const auto& tab : m_editors)
    vec.push_back(tab.textedit->document()->toPlainText());
  return vec;
}

void MultiScriptDialog::setText(int idx, const QString& str)
{
  SCORE_ASSERT(idx >= 0);
  SCORE_ASSERT(std::size_t(idx) < m_editors.size());

  auto textEdit = m_editors[idx].textedit;
  if(str != textEdit->document()->toPlainText())
  {
    textEdit->setPlainText(str);
  }
}

void MultiScriptDialog::setError(const QString& str)
{
  m_error->setPlainText(str);
}

void MultiScriptDialog::clearError()
{
  m_error->clear();
}

void MultiScriptDialog::openInExternalEditor(const QString& editorPath)
{
#if QT_CONFIG(process)
  if(editorPath.isEmpty())
  {
    QMessageBox::warning(
        this, tr("Error"), tr("no 'Default editor' configured in score settings"));
    return;
  }

  if(!QFile::exists(editorPath))
  {
    QMessageBox::warning(
        this, tr("Error"), tr("the configured external editor does not exist."));
    return;
  }

  QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QDir dir(tempDir);
  QStringList openedFiles;

  for(int i = 0; i < m_tabs->count(); ++i)
  {
    QString tabName = m_tabs->tabText(i);
    QString tempFile = tempDir + "/" + tabName + ".js";

    QFile file(tempFile);
    if(!file.open(QIODevice::WriteOnly))
    {
      QMessageBox::warning(this, tr("Error"), tr("failed to create temporary files."));
      return;
    }

    QWidget* widget = m_tabs->widget(i);
    QTextEdit* textedit = qobject_cast<QTextEdit*>(widget);

    if(textedit)
    {
      QString content = textedit->document()->toPlainText();
      file.write(content.toUtf8());
    }

    openedFiles.append(tempFile);
  }

  if(!openedFiles.isEmpty() && !QProcess::startDetached(editorPath, openedFiles))
  {
    QMessageBox::warning(this, tr("Error"), tr("Failed to launch external editor"));
  }
#endif
}

}
