#pragma once
#include <QProcess>
#include <QQmlEngine>

#include <verdigris>

namespace JS
{
class QmlProcess : public QProcess
{
  W_OBJECT(QmlProcess)
public:
  explicit QmlProcess(QObject* parent = nullptr);
  ~QmlProcess() override;

  bool running() const { return state() == Running; }

  // Getters wrapping QProcess methods for verdigris binding
  QString program() const { return QProcess::program(); }
  QStringList arguments() const { return QProcess::arguments(); }
  QString workingDirectory() const { return QProcess::workingDirectory(); }
  QProcess::ProcessState processState() const { return QProcess::state(); }
  QProcess::ProcessError processError() const { return QProcess::error(); }
  int exitCode() const { return QProcess::exitCode(); }
  QProcess::ExitStatus exitStatus() const { return QProcess::exitStatus(); }
  qint64 processId() const { return QProcess::processId(); }

  QString standardOutput() const { return m_standardOutput; }
  QString standardError() const { return m_standardError; }

  // Invokable methods for QML
  void start();
  W_INVOKABLE(start);
  void stop();
  W_INVOKABLE(stop);
  void kill();
  W_INVOKABLE(kill);
  void clearOutput();
  W_INVOKABLE(clearOutput);
  void write(const QString& data);
  W_INVOKABLE(write);

  // Custom setters that wrap QProcess setters and emit signals
  void setProgram(const QString& program);
  void setArguments(const QStringList& arguments);
  void setWorkingDirectory(const QString& dir);

  // Signals
  void started() W_SIGNAL(started);
  void finished(int exitCode, QProcess::ExitStatus status)
      W_SIGNAL(finished, exitCode, status);

  void programChanged() W_SIGNAL(programChanged);
  void argumentsChanged() W_SIGNAL(argumentsChanged);
  void workingDirectoryChanged() W_SIGNAL(workingDirectoryChanged);
  void processStateChanged() W_SIGNAL(processStateChanged);
  void runningChanged() W_SIGNAL(runningChanged);
  void processErrorChanged() W_SIGNAL(processErrorChanged);
  void exitCodeChanged() W_SIGNAL(exitCodeChanged);
  void exitStatusChanged() W_SIGNAL(exitStatusChanged);
  void standardOutputChanged() W_SIGNAL(standardOutputChanged);
  void standardErrorChanged() W_SIGNAL(standardErrorChanged);

  void outputLineReceived(const QString& line) W_SIGNAL(outputLineReceived, line);
  void errorLineReceived(const QString& line) W_SIGNAL(errorLineReceived, line);

  void lineReceived(const QString& line, bool isError)
      W_SIGNAL(lineReceived, line, isError);

  void onStarted();
  W_SLOT(onStarted);
  void onReadyReadStandardOutput();
  W_SLOT(onReadyReadStandardOutput);
  void onReadyReadStandardError();
  W_SLOT(onReadyReadStandardError);
  void onStateChanged(QProcess::ProcessState newState);
  W_SLOT(onStateChanged);
  void onFinished(int code, QProcess::ExitStatus status);
  W_SLOT(onFinished);
  void onErrorOccurred(QProcess::ProcessError error);
  W_SLOT(onErrorOccurred);

  W_PROPERTY(QString, program READ program WRITE setProgram NOTIFY programChanged)
  W_PROPERTY(
      QStringList, arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
  W_PROPERTY(
      QString, workingDirectory READ workingDirectory WRITE setWorkingDirectory NOTIFY
                   workingDirectoryChanged)

  // State
  W_PROPERTY(ProcessState, processState READ processState NOTIFY processStateChanged)
  W_PROPERTY(bool, running READ running NOTIFY runningChanged)
  W_PROPERTY(ProcessError, processError READ processError NOTIFY processErrorChanged)
  W_PROPERTY(int, exitCode READ exitCode NOTIFY exitCodeChanged)
  W_PROPERTY(ExitStatus, exitStatus READ exitStatus NOTIFY exitStatusChanged)
  W_PROPERTY(qint64, processId READ processId NOTIFY started)

  // Accumulated output (convenience for QML bindings)
  W_PROPERTY(QString, standardOutput READ standardOutput NOTIFY standardOutputChanged)
  W_PROPERTY(QString, standardError READ standardError NOTIFY standardErrorChanged)

private:
  QString m_standardOutput;
  QString m_standardError;
  QByteArray m_stdoutBuffer;
  QByteArray m_stderrBuffer;
};
}

W_REGISTER_ARGTYPE(QProcess::ProcessError)
W_REGISTER_ARGTYPE(QProcess::ExitStatus)
W_REGISTER_ARGTYPE(QProcess::ProcessState)
