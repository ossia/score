#pragma once

// The score application binary run on a --script, for tests whose subject is
// the effect of a real script run on a live application. The binary comes from
// the SCORE_APP_BINARY definition of the test target.

#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace score::test::app
{

inline QString binary()
{
#if defined(SCORE_APP_BINARY)
  return QStringLiteral(SCORE_APP_BINARY);
#else
  return {};
#endif
}

inline bool binary_available()
{
  const QString b = binary();
  return !b.isEmpty() && QFile::exists(b);
}

//! A display a GPU window can be created on: not the offscreen platform.
inline bool has_display()
{
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == QLatin1String("offscreen"))
    return false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  return qEnvironmentVariableIsSet("DISPLAY") || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#else
  return true;
#endif
}

struct options
{
  //! Documents to load, given before the flags.
  QStringList documents;
  //! Window devices rendered offscreen (SCORE_FORCE_OFFSCREEN_WINDOW), on the
  //! inherited display. Empty: no display at all, the offscreen platform.
  QString offscreen_windows;
  int timeout_ms{180000};
};

struct result
{
  bool finished{};
  bool crashed{true};
  int exit_code{-1};
  //! stdout and stderr, also when the run timed out.
  QString output;
};

inline result run_script(const QString& script, const options& opts = {})
{
  auto env = QProcessEnvironment::systemEnvironment();
  if(opts.offscreen_windows.isEmpty())
  {
    env.remove("DISPLAY");
    env.remove("WAYLAND_DISPLAY");
    env.insert("QT_QPA_PLATFORM", "offscreen");
  }
  else
  {
    env.insert("SCORE_FORCE_OFFSCREEN_WINDOW", opts.offscreen_windows);
  }
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // Not the 6666 / 9999 of the developer's own session (see App.hpp).
  if(!env.contains("SCORE_LOCAL_OSC_PORT"))
    env.insert("SCORE_LOCAL_OSC_PORT", "0");
  if(!env.contains("SCORE_LOCAL_WS_PORT"))
    env.insert("SCORE_LOCAL_WS_PORT", "0");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      binary(), opts.documents
                    + QStringList{"--no-gui", "--no-restore", "--wait", "0", "--script", script});

  result r;
  r.finished = p.waitForStarted(30000) && p.waitForFinished(opts.timeout_ms);
  if(!r.finished)
  {
    p.kill();
    p.waitForFinished(5000);
  }
  r.output = QString::fromUtf8(p.readAll());
  if(r.finished)
  {
    r.crashed = p.exitStatus() != QProcess::NormalExit;
    r.exit_code = p.exitCode();
  }
  return r;
}

//! Writes \p body to \p name in \p dir and returns its path.
inline QString write_file(const QTemporaryDir& dir, const QString& name, const QByteArray& body)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(body);
  return path;
}

//! \p s as the contents of a JS string literal.
inline QString js_string(QString s)
{
  s.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
  return QLatin1Char('"') + s + QLatin1Char('"');
}

}
