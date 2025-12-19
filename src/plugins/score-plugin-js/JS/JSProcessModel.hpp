#pragma once
#include <Process/Process.hpp>
#include <Process/Script/ScriptProcess.hpp>

#include <JS/JSProcessMetadata.hpp>
#include <JS/Qml/QmlObjects.hpp>

#include <QFileSystemWatcher>
#include <QQmlComponent>
#include <QQmlEngine>

#include <score_plugin_js_export.h>

#include <memory>
#include <verdigris>
class QQuickItem;

namespace JS
{
class Script;
class ProcessModel;

struct QmlSource {
  QmlSource() = default;
  ~QmlSource() = default;
  QmlSource(const QmlSource&) = default;
  QmlSource(QmlSource&&) = default;

  QmlSource(const QString& execution, const QString& ui)
      : execution{execution}
      , ui{ui}
  {
  }

  QmlSource(const std::vector<QString>& vec)
  {
    SCORE_ASSERT(vec.size() == 2);
    execution = vec[0];
    ui = vec[1];
  }

  QmlSource& operator=(const QmlSource&) = default;
  QmlSource& operator=(QmlSource&&) = default;

  QString execution;
  QString ui;

  struct MemberSpec
  {
    const QString name;
    const QString QmlSource::*pointer;
    const std::string_view language{};
  };

  static const inline std::array<MemberSpec, 2> specification{
                                                              MemberSpec{QObject::tr("Execution"), &QmlSource::execution, "QML"},
                                                              MemberSpec{QObject::tr("GUI"), &QmlSource::ui, "QML"},
  };

  friend bool operator!=(const QmlSource& lhs, const std::vector<QString>& rhs) {
    return rhs.size() != 2 || lhs.execution != rhs[0]|| lhs.ui != rhs[1];
  }
  friend bool operator!=(const std::vector<QString>& rhs, const QmlSource& lhs) {
    return rhs.size() != 2 || lhs.execution != rhs[0]|| lhs.ui != rhs[1];
  }
};


struct ComponentCache
{
  struct Cache
  {
    QByteArray key;
    std::unique_ptr<QQmlComponent> component{};
    std::unique_ptr<JS::Script> object{};
  };

  JS::Script*
  getExecution(const JS::ProcessModel& process, const QByteArray& str, bool isFile) noexcept;
  QQmlComponent*
  getUi(const JS::ProcessModel& process, const QByteArray& str, bool isFile) noexcept;
  const Cache* tryGet(const QByteArray& str, bool isFile) const noexcept;

  ComponentCache();
  ~ComponentCache();

private:
  std::vector<Cache> m_map;

};

class SCORE_PLUGIN_JS_EXPORT ProcessModel final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(JS::ProcessModel)
  W_OBJECT(ProcessModel)
public:
  bool hasExternalUI() const noexcept { return bool(m_ui_component); }

  explicit ProcessModel(
      const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  void setExecutionScript(const QString& script);
  const QString& executionScript() const noexcept { return m_program.execution; }
  void executionScriptOk() W_SIGNAL(executionScriptOk);
  void executionScriptChanged(const QString& arg_1) W_SIGNAL(executionScriptChanged, arg_1);
  const QByteArray& qmlData() const noexcept { return m_qmlData; }

  void setUiScript(const QString& script);
  const QString& uiScript() const noexcept { return m_program.ui; }
  void uiScriptOk() W_SIGNAL(uiScriptOk);
  void uiScriptChanged(const QString& arg_1) W_SIGNAL(uiScriptChanged, arg_1);

  JS::Script* currentExecutionObject() const noexcept;
  QQuickItem* currentUI() const noexcept;
  bool isGpu() const noexcept;
  bool hasUi() const noexcept;
  QWidget* createWindowForUI(const score::DocumentContext& ctx,
                             QWidget* parent) const noexcept;

  ~ProcessModel() override;

  bool validate(const std::vector<QString>& str) const noexcept;
  void errorMessage(const QString& arg_2) const
      W_SIGNAL(errorMessage, arg_2);

  const QmlSource& program() const noexcept { return m_program; }
  [[nodiscard]] Process::ScriptChangeResult setProgram(const QmlSource& f);
  void programChanged() W_SIGNAL(programChanged);


  void uiToExecution(const QVariant& v) W_SIGNAL(uiToExecution, v);
  void executionToUi(const QVariant& v) W_SIGNAL(executionToUi, v);

  PROPERTY(QString, executionScript READ executionScript WRITE setExecutionScript NOTIFY executionScriptChanged)
  PROPERTY(QString, uiScript READ uiScript WRITE setUiScript NOTIFY uiScriptChanged)
  PROPERTY(JS::QmlSource, program READ program WRITE setProgram NOTIFY programChanged)
private:
  QString effect() const noexcept override;
  void loadPreset(const Process::Preset& preset) override;
  Process::Preset savePreset() const noexcept override;
  Process::ScriptChangeResult setQmlData(const QByteArray&, bool isFile);

  QmlSource m_program;
  QByteArray m_qmlData;

  QQmlComponent* m_ui_component{};
  QObject* m_ui_object{};

  mutable ComponentCache m_cache;
  bool m_isFile{};
};
}
