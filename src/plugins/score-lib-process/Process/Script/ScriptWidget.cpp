#include <Process/Script/ScriptWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/File.hpp>

#include <QCXXHighlighter>
#include <QCodeEditor>
#include <QFaustCompleter>
#include <QFaustHighlighter>
#include <QFile>
#include <QGLSLCompleter>
#include <QGLSLHighlighter>
#include <QJSCompleter>
#include <QJSHighlighter>
#include <QMainWindow>
#include <QRegularExpression>
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

std::pair<QStyleSyntaxHighlighter*, QCompleter*>
getLanguageStyle(const std::string_view language)
{
  if(language == "glsl" || language == "Glsl" || language == "GLSL")
  {
    return {new QGLSLHighlighter, new QGLSLCompleter};
  }
  else if(language == "js" || language == "Js" || language == "JS")
  {
    return {new QJSHighlighter, new QJSCompleter};
  }
  else if(language == "qml" || language == "Qml" || language == "QML")
  {
    return {new QJSHighlighter, new QJSCompleter};
  }
  else if(language == "faust" || language == "Faust")
  {
    return {new QFaustHighlighter, new QFaustCompleter};
  }
  else // C, C++, ...
  {
    return {new QCXXHighlighter, nullptr};
  }
}

}

static QByteArray themeSource()
{
  // A copy, not a mapping: the file is closed when this returns
  QFile fl(":/drakula.xml");
  if(fl.open(QIODevice::ReadOnly))
    return fl.readAll();
  qDebug() << "Could not load style:" << fl.fileName();
  return {};
}

QSyntaxStyle* scriptStyle()
{
  static bool tried_to_load = false;
  static QSyntaxStyle style;
  if(!tried_to_load)
  {
    if(const auto src = themeSource(); !src.isEmpty())
      style.load(src);
    tried_to_load = true;
  }
  return &style;
}

QSyntaxStyle* overlayScriptStyle()
{
  static bool tried_to_load = false;
  static QSyntaxStyle style;
  if(!tried_to_load)
  {
    if(auto src = QString::fromUtf8(themeSource()); !src.isEmpty())
    {
      // QColor reads #AARRGGBB: alpha 0 for the text background, which also
      // fills the line number area; 10% for the current line.
      static const QRegularExpression text{
          QStringLiteral(R"((name="Text"[^/>]*background=")#([0-9a-fA-F]{6}"))")};
      static const QRegularExpression currentLine{
          QStringLiteral(R"((name="CurrentLine"[^/>]*background=")#([0-9a-fA-F]{6}"))")};
      src.replace(text, QStringLiteral("\\1#00\\2"));
      src.replace(currentLine, QStringLiteral("\\1#1a\\2"));
      style.load(src);
    }
    tried_to_load = true;
  }
  return &style;
}

QTextEdit* createScriptWidget(const std::string_view language)
{
  auto edit = new QCodeEditor{};
  // px, not pt: pt would shrink on macOS' 72 DPI.
  auto font = QFont("IBM Plex Mono");
  font.setPixelSize(13);
  font.setFixedPitch(true);
  font.setStyleStrategy(QFont::PreferAntialias);
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

  edit->setSyntaxStyle(scriptStyle());

  setTabWidth(*edit, 4);

  return edit;
}
}
