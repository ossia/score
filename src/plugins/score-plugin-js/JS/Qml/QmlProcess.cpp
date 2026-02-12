#include "QmlProcess.hpp"

#include <QTimer>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::QmlProcess)
namespace JS
{
QmlProcess::QmlProcess(QObject* parent)
    : QProcess(parent)
{
  connect(this, &QProcess::started, this, &QmlProcess::onStarted);
  connect(
      this, &QProcess::readyReadStandardOutput, this,
      &QmlProcess::onReadyReadStandardOutput);
  connect(
      this, &QProcess::readyReadStandardError, this,
      &QmlProcess::onReadyReadStandardError);
  connect(this, &QProcess::stateChanged, this, &QmlProcess::onStateChanged);
  connect(
      this, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
      &QmlProcess::onFinished);
  connect(this, &QProcess::errorOccurred, this, &QmlProcess::onErrorOccurred);
}

QmlProcess::~QmlProcess()
{
  if(state() != NotRunning)
  {
    terminate();
    waitForFinished(1000);
  }
}

void QmlProcess::start()
{
  QProcess::start(program(), arguments());
}

void QmlProcess::stop()
{
  terminate();
  QTimer::singleShot(3000, this, [this]() {
    if(state() != NotRunning)
      kill();
  });
}

void QmlProcess::kill()
{
  QProcess::kill();
}

void QmlProcess::write(const QString& data)
{
  QProcess::write(data.toUtf8());
}

void QmlProcess::clearOutput()
{
  m_standardOutput.clear();
  m_standardError.clear();
  m_stdoutBuffer.clear();
  m_stderrBuffer.clear();
  standardOutputChanged();
  standardErrorChanged();
}

void QmlProcess::onStarted()
{
  started();
}

void QmlProcess::onReadyReadStandardOutput()
{
  m_stdoutBuffer.append(readAllStandardOutput());

  // Process complete lines
  int idx;
  while((idx = m_stdoutBuffer.indexOf('\n')) != -1)
  {
    QString line = QString::fromUtf8(m_stdoutBuffer.left(idx));
    m_stdoutBuffer.remove(0, idx + 1);

    m_standardOutput.append(line + "\n");
    outputLineReceived(line);
    lineReceived(line, false);
  }
  standardOutputChanged();
}

void QmlProcess::onReadyReadStandardError()
{
  m_stderrBuffer.append(readAllStandardError());

  int idx;
  while((idx = m_stderrBuffer.indexOf('\n')) != -1)
  {
    QString line = QString::fromUtf8(m_stderrBuffer.left(idx));
    m_stderrBuffer.remove(0, idx + 1);

    m_standardError.append(line + "\n");
    errorLineReceived(line);
    lineReceived(line, true);
  }
  standardErrorChanged();
}

void QmlProcess::onStateChanged(QProcess::ProcessState newState)
{
  processStateChanged();
  runningChanged();
}

void QmlProcess::onFinished(int code, QProcess::ExitStatus status)
{
  // Flush remaining buffered output
  if(!m_stdoutBuffer.isEmpty())
  {
    QString line = QString::fromUtf8(m_stdoutBuffer);
    m_standardOutput.append(line);
    outputLineReceived(line);
    lineReceived(line, false);
    standardOutputChanged();
    m_stdoutBuffer.clear();
  }
  if(!m_stderrBuffer.isEmpty())
  {
    QString line = QString::fromUtf8(m_stderrBuffer);
    m_standardError.append(line);
    errorLineReceived(line);
    lineReceived(line, true);
    standardErrorChanged();
    m_stderrBuffer.clear();
  }

  exitCodeChanged();
  exitStatusChanged();
  finished(code, status);
}

void QmlProcess::onErrorOccurred(QProcess::ProcessError error)
{
  processErrorChanged();
}

void QmlProcess::setProgram(const QString& prog)
{
  if(program() == prog)
    return;
  QProcess::setProgram(prog);
  programChanged();
}

void QmlProcess::setArguments(const QStringList& args)
{
  if(arguments() == args)
    return;
  QProcess::setArguments(args);
  argumentsChanged();
}

void QmlProcess::setWorkingDirectory(const QString& dir)
{
  if(workingDirectory() == dir)
    return;
  QProcess::setWorkingDirectory(dir);
  workingDirectoryChanged();
}
}
