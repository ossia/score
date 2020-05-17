#pragma once
#include <QFile>
#include <QDir>
#include <QFileSystemWatcher>
#include <QObject>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <vector>

namespace score
{

struct StyleLoader : public QObject
{
  std::vector<QString> filesToRead;

#if defined(SCORE_SOURCE_DIR)
  std::vector<QFileSystemWatcher*> watchers;
#endif

  StyleLoader()
  {
#if defined(SCORE_SOURCE_DIR)
    QString prefix = QString(SCORE_SOURCE_DIR) + "/src/lib/resources";
    if(!QDir::root().exists(prefix))
      prefix = ":";
#else
    QString prefix = ":";
#endif
  #if defined(_WIN32)
    addFile(prefix + "/style/windows.qss");
  #elif defined(__APPLE__)
    addFile(prefix + "/style/macos.qss");
  #else
    addFile(prefix + "/style/linux.qss");
  #endif
    addFile(prefix + "/qsimpledarkstyle.qss");
    addFile(prefix + "/style/spinbox.qss");
    addFile(prefix + "/style/scrollbars.qss");
    addFile(prefix + "/style/lineedit.qss");
  }

  void addFile(const QString& str)
  {
    filesToRead.push_back(str);
#if defined(SCORE_SOURCE_DIR)
    watchers.push_back(new QFileSystemWatcher{this});
    auto w = watchers.back();
    connect(w, &QFileSystemWatcher::fileChanged,
            this, [=] {
      QTimer::singleShot(100, [=] {
        on_styleChanged();
        w->addPath(str);
      });
    });
    w->addPath(str);
#endif
  }

  QByteArray readStyleSheet() const
  {
    QByteArray ss;

    auto readFile = [] (QString s) {
      QFile f{s};

      if(!f.open(QFile::ReadOnly))
      {
        qDebug() << "Warning : could not read style file: " << s;
        return QByteArray{};
      }

      return f.readAll();
    };

    for(const auto& path : filesToRead)
    {
      ss += readFile(path);
    }
    return ss;
  }

  void on_styleChanged()
  {
#ifndef QT_NO_STYLE_STYLESHEET
  //  qApp->setStyleSheet(readStyleSheet());
#endif
  }
};


}
