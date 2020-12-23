#include <Process/Script/ScriptWidget.hpp>
#include <QCodeEditor>
#include <QCXXHighlighter>
#include <QGLSLHighlighter>
#include <QJSHighlighter>
#include <QGLSLCompleter>
#include <QSyntaxStyle>
#include <QMainWindow>
#include <score/application/GUIApplicationContext.hpp>

namespace Process
{
namespace
{
void setTabWidth(QTextEdit& edit, int spaceCount)
{
  const QString spaces(spaceCount, QChar(' '));
  const QFontMetrics metrics(edit.font());
#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
  const qreal width = metrics.boundingRect(spaces).width();
  edit.setTabStopWidth(width);
#else
  edit.setTabStopDistance(metrics.horizontalAdvance(spaces));
#endif
}

std::pair<QStyleSyntaxHighlighter*, QCompleter*> getLanguageStyle(const std::string_view language)
{
  auto init = [] (auto t) { t->setParent(score::GUIAppContext().mainWindow); return t; };
  if(language == "glsl" || language == "Glsl" || language == "GLSL")
  {
    static auto highlight = init(new QGLSLHighlighter);
    static auto completer = init(new QGLSLCompleter);
    return {highlight, completer};
  }
  else if(language == "js" || language == "Js" || language == "JS")
  {
    static auto highlight = init(new QJSHighlighter);
    return {highlight, nullptr};
  }
  else // C, C++, ...
  {
    static auto highlight = init(new QCXXHighlighter);
    return {highlight, nullptr};
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
