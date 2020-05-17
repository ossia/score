#include "ScriptEditor.hpp"

#include <QCXXHighlighter>
#include <QCodeEditor>
#include <QDialogButtonBox>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSyntaxStyle>
#include <QVBoxLayout>
namespace Process
{

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
}
