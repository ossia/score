#include "ScriptEditor.hpp"
#include "MultiScriptEditor.hpp"

#include <QCXXHighlighter>
#include <QCodeEditor>
#include <QDialogButtonBox>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxStyle>
#include <QTabWidget>
#include <QVBoxLayout>
namespace Process
{
namespace
{
void setTabWidth(QTextEdit& edit, int spaceCount)
{
  const QString spaces(spaceCount, QChar(' '));
  const QFontMetrics metrics(edit.font());
  edit.setTabStopDistance(metrics.horizontalAdvance(spaces));
}
}

ScriptDialog::ScriptDialog(const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}, m_context{ctx}
{
  this->setBaseSize(800, 300);
  this->setWindowFlag(Qt::WindowCloseButtonHint, false);
  auto lay = new QVBoxLayout{this};
  this->setLayout(lay);

  m_textedit = new QCodeEditor{this};

  {
    QFile fl(":/drakula.xml");

    if (!fl.open(QIODevice::ReadOnly))
    {
      return;
    }

    auto style = new QSyntaxStyle(this);
    if (!style->load(fl.readAll()))
    {
      delete style;
      return;
    }
    m_textedit->setSyntaxStyle(style);
  }

  m_textedit->setHighlighter(new QCXXHighlighter);
  setTabWidth(*m_textedit, 4);

  m_error = new QPlainTextEdit{this};
  m_error->setReadOnly(true);
  m_error->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

  lay->addWidget(m_textedit);
  lay->addWidget(m_error);
  lay->setStretch(0, 3);
  lay->setStretch(1, 1);
  auto bbox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Reset | QDialogButtonBox::Close, this};
  bbox->button(QDialogButtonBox::Ok)->setText(tr("Compile"));
  bbox->button(QDialogButtonBox::Reset)->setText(tr("Clear log"));
  connect(bbox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [=] {
    m_error->clear();
  });
  lay->addWidget(bbox);

  connect(bbox, &QDialogButtonBox::accepted, this, &ScriptDialog::on_accepted);
  connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString ScriptDialog::text() const noexcept
{
  return m_textedit->document()->toPlainText();
}

void ScriptDialog::setText(const QString& str)
{
  if (str != text())
  {
    m_textedit->setPlainText(str);
  }
}

void ScriptDialog::setError(int line, const QString& str)
{
  m_error->setPlainText(str);
}








MultiScriptDialog::MultiScriptDialog(const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}, m_context{ctx}
{
  this->setBaseSize(800, 300);
  this->setWindowFlag(Qt::WindowCloseButtonHint, false);
  auto lay = new QVBoxLayout{this};
  this->setLayout(lay);

  m_tabs = new QTabWidget;
  lay->addWidget(m_tabs);

  m_error = new QPlainTextEdit;
  lay->addWidget(m_error);

  auto bbox = new QDialogButtonBox{
      QDialogButtonBox::Ok | QDialogButtonBox::Reset | QDialogButtonBox::Close, this};

  lay->addWidget(bbox);

  bbox->button(QDialogButtonBox::Ok)->setText(tr("Compile"));
  bbox->button(QDialogButtonBox::Reset)->setText(tr("Clear log"));
  connect(bbox->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [=] {
    m_error->clear();
  });
  connect(bbox, &QDialogButtonBox::accepted, this, &MultiScriptDialog::on_accepted);
  connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void MultiScriptDialog::addTab(const QString &name, const QString &text)
{
  auto widg = new QWidget;
  auto lay = new QVBoxLayout{widg};
  auto m_textedit = new QCodeEditor{this};
  m_textedit->setText(text);

  // TODO share that with scriptdialog, and load it only once
  {
    QFile fl(":/drakula.xml");

    if (fl.open(QIODevice::ReadOnly))
    {
      auto style = new QSyntaxStyle(this);
      if (!style->load(fl.readAll()))
      {
        delete style;
      }
      else
      {
        m_textedit->setSyntaxStyle(style);
      }
    }
  }

  m_textedit->setHighlighter(new QCXXHighlighter);
  setTabWidth(*m_textedit, 4);

  auto m_error = new QPlainTextEdit{this};
  m_error->setReadOnly(true);
  m_error->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

  lay->addWidget(m_textedit);
  lay->addWidget(m_error);
  lay->setStretch(0, 3);
  lay->setStretch(1, 1);

  m_tabs->addTab(widg, name);
  m_editors.push_back({m_textedit});
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
  if (str != textEdit->document()->toPlainText())
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

}
