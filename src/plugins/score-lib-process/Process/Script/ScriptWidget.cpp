#include <Process/Script/ScriptWidget.hpp>
#include <QCodeEditor>
#include <QCXXHighlighter>
#include <QGLSLHighlighter>
#include <QJSHighlighter>
#include <QGLSLCompleter>
#include <QSyntaxStyle>

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

std::pair<QStyleSyntaxHighlighter*, QCompleter*> getLanguageStyle(const std::string_view language)
{
  if(language == "glsl" || language == "Glsl" || language == "GLSL")
  {
    static QGLSLHighlighter highlight;
    static QGLSLCompleter completer;
    return {&highlight, &completer};
  }
  else if(language == "js" || language == "Js" || language == "JS")
  {
    static QJSHighlighter highlight;
    return {&highlight, nullptr};
  }
  else // C, C++, ...
  {
    static QCXXHighlighter highlight;
    return {&highlight, nullptr};
  }
}

QSyntaxStyle* getStyle()
{
  static bool tried_to_load = false;
  static QSyntaxStyle style;
  if(!tried_to_load)
  {
    QFile fl(":/drakula.xml");

    if (fl.open(QIODevice::ReadOnly))
      style.load(fl.readAll());

    tried_to_load = true;
  }
  return &style;
}
}

QCodeEditor* createScriptWidget(const std::string_view language)
{
  auto edit = new QCodeEditor{};

  auto [highlight, complete] = getLanguageStyle(language);
  edit->setHighlighter(highlight);
  edit->setCompleter(complete);

  edit->setSyntaxStyle(getStyle());

  setTabWidth(*edit, 4);
  return edit;
}
}
