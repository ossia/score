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

  QString standardOutput() const { return m_standardOutput; }
  QString standardError() const { return m_standardError; }

  // Invokable methods for QML
  void start();
  W_INVOKABLE(start);
  void stop();
  W_INVOKABLE(stop)
  void clearOutput();
  W_INVOKABLE(clearOutput);

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
  W_PROPERTY(ProcessState, processState READ state NOTIFY processStateChanged)
  W_PROPERTY(bool, running READ running NOTIFY runningChanged)
  W_PROPERTY(ProcessError, processError READ error NOTIFY processErrorChanged)
  W_PROPERTY(int, exitCode READ exitCode NOTIFY exitCodeChanged)
  W_PROPERTY(ExitStatus, exitStatus READ exitStatus NOTIFY exitStatusChanged)

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
