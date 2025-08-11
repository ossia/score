#include <JS/Qml/Utils.hpp>

#include <score/tools/File.hpp>
#include <score/tools/ThreadPool.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QFontMetrics>
#include <QProcess>
#include <QTemporaryFile>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::JsUtils)
namespace JS
{

QByteArray JsUtils::readFile(QString path)
{
  if(auto doc = score::AppContext().currentDocument())
    path = score::locateFilePath(path, *doc);

  QFile f(path);
  if(f.open(QIODevice::ReadOnly))
    return f.readAll();
  return {};
}

void JsUtils::shell(QString cmd, QJSValue onFinish)
{
#if QT_CONFIG(process)
  const QString sh = "bash";

  QString filename;

  {
    QTemporaryFile f;
    f.setAutoRemove(false);
    if(!f.open())
      return;
    filename = f.fileName();
    f.setPermissions(
        QFileDevice::WriteOwner | QFileDevice::ReadOwner | QFileDevice::ExeOwner);
    f.write(cmd.toUtf8());
    f.close();
  }

  auto& tp = score::TaskPool::instance();
  tp.post([onFinish = std::make_shared<QJSValue>(onFinish), filename, sh] {
    QList<QByteArray> strs;
    int code{};

    QProcess p;
    p.setProgram(sh);
    p.setArguments({"-c", filename});
    p.start();
    p.waitForStarted();
    p.waitForFinished();
    code = p.exitCode();
    strs.push_back(p.readAllStandardOutput());
    strs.push_back(p.readAllStandardError());

    QMetaObject::invokeMethod(qApp, [=] {
      QFile::remove(filename);

      if(onFinish->isCallable())
        onFinish->call(
            QJSValueList() << code << QString::fromUtf8(strs[0])
                           << QString::fromUtf8(strs[1]));
    });
  });
  //Util.shell("echo toto", (code, stdout, stderr) => { console.log(code, stdout, stderr); })
#endif
}

QString JsUtils::layoutTextLines(QString text, QString font, int pointSize, int maxWidth)
{
  if(text.isEmpty())
    return text;
  if(ossia::all_of(text, [](const QChar& c) { return c.isLetterOrNumber(); }))
    return text;
  if(pointSize <= 0)
    return text;
  if(maxWidth <= 0)
    maxWidth = 0;

  QString cur = text;
  QFontMetrics m{QFont(font, pointSize)};

  if(m.boundingRect(cur).width() < maxWidth)
    return cur;

  int last_inserted_linebreak = 0;
  while(true)
  {
    // Go to the last line break
    int start_search_index = last_inserted_linebreak;
    int last_line_break = cur.lastIndexOf('\n', last_inserted_linebreak);
    if(last_line_break != -1)
    {
      start_search_index = last_line_break + 1;
      if(start_search_index >= (cur.size() - 1))
        break;
    }

    int min_index_for_break = start_search_index;
    int last_breakable_char = -1;
    bool broken = false;
    // Try to find the most characters that fit in maxWidth
    for(int j = start_search_index; j < cur.size(); j++)
    {
      if(!cur[j].isLetterOrNumber())
        last_breakable_char = j;

      QString line = cur.mid(start_search_index, j - start_search_index);
      if(m.boundingRect(line).width() < maxWidth)
      {
        min_index_for_break++;
      }
      else
      {
        // We have to break.
        if(last_breakable_char == -1)
        {
          break; // Go to the "No more characters to break" loop
        }
        else
        {
          cur.insert(last_breakable_char + 1, '\n');
          last_inserted_linebreak = last_breakable_char + 1;
          broken = true;
          break;
        }
      }
    }
    if(broken)
      continue;

    // We're actually good
    if(m.boundingRect(cur.mid(start_search_index)).width() < maxWidth)
      break;

    // No more characters to break, we continue until the next one
    if(last_breakable_char == -1)
    {
      for(int j = min_index_for_break; j < cur.size(); j++)
      {
        if(!cur[j].isLetterOrNumber())
        {
          // Insert a break after it
          cur.insert(j + 1, '\n');
          last_inserted_linebreak = j + 1;
          broken = true;
          break;
        }
      }
      if(broken)
        continue;

      // No more characters to break at all, end there
      break;
    }

    // To make sure that we eventually terminate:
    last_inserted_linebreak++;
    if(last_inserted_linebreak >= cur.size() - 1)
      break;
  }

  return cur;
}

}
