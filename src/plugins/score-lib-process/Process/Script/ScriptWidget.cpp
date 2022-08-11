#include <Process/Script/ScriptWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/File.hpp>

#include <QCXXHighlighter>
#include <QCodeEditor>
#include <QFile>
#include <QGLSLCompleter>
#include <QGLSLHighlighter>
#include <QJSHighlighter>
#include <QMainWindow>
#include <QSyntaxStyle>

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

std::pair<QStyleSyntaxHighlighter*, QCompleter*>
getLanguageStyle(const std::string_view language)
{
  if(language == "glsl" || language == "Glsl" || language == "GLSL")
  {
    return {new QGLSLHighlighter, new QGLSLCompleter};
  }
  else if(language == "js" || language == "Js" || language == "JS")
  {
    return {new QJSHighlighter, nullptr};
  }
  else // C, C++, ...
  {
    return {new QCXXHighlighter, nullptr};
  }
}

QSyntaxStyle* getStyle()
{
  static bool tried_to_load = false;
  static QSyntaxStyle style;
  if(!tried_to_load)
  {
    QFile fl(":/drakula.xml");

    if(fl.open(QIODevice::ReadOnly))
      style.load(score::mapAsByteArray(fl));

    tried_to_load = true;
  }
  return &style;
}
}

QTextEdit* createScriptWidget(const std::string_view language)
{
  auto edit = new QCodeEditor{};
  auto font = QFont("Source Code Pro", 10);
  font.setFixedPitch(true);
  font.setHintingPreference(QFont::HintingPreference::PreferVerticalHinting);
  edit->setFont(font);

  auto [highlight, complete] = getLanguageStyle(language);

  if(highlight)
  {
    edit->setHighlighter(highlight);
    highlight->setParent(edit);
  }
  if(complete)
  {
    edit->setCompleter(complete);
    complete->setParent(edit);
  }

  edit->setSyntaxStyle(getStyle());

  setTabWidth(*edit, 4);

  return edit;
}
}
