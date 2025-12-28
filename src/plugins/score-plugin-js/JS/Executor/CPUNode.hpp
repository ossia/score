#pragma once

#include <JS/Executor/JSAPIWrapper.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/Qml/Metatypes.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <JS/Qml/ValueTypes.Qt6.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_edge_helpers.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/detail/ssize.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/js_utilities.hpp>
#include <ossia-qt/time.hpp>
#include <ossia-qt/token_request.hpp>
namespace ossia::qt
{
class qml_engine_functions;
}

namespace JS
{
class js_node final : public ossia::graph_node
{
public:
  explicit js_node(ossia::execution_state& st);
  ~js_node();

  [[nodiscard]] std::string label() const noexcept override { return "javascript"; }

  void run(const ossia::token_request& t, ossia::exec_state_facade) noexcept override;
  void clear() noexcept override;
  void setupComponent();
  void setScript(const QString& val);

  ossia::execution_state& m_st;

  std::shared_ptr<QQmlEngine> m_engine{};
  QQmlContext* m_context{};
  std::vector<Inlet*> m_jsInlets;
  std::vector<std::pair<ControlInlet*, ossia::inlet_ptr>> m_ctrlInlets;
  std::vector<std::pair<Impulse*, ossia::inlet_ptr>> m_impulseInlets;
  std::vector<std::pair<ValueInlet*, ossia::inlet_ptr>> m_valInlets;
  std::vector<std::pair<ValueOutlet*, ossia::outlet_ptr>> m_valOutlets;
  std::vector<std::pair<AudioInlet*, ossia::inlet_ptr>> m_audInlets;
  std::vector<std::pair<AudioOutlet*, ossia::outlet_ptr>> m_audOutlets;
  std::vector<std::pair<MidiInlet*, ossia::inlet_ptr>> m_midInlets;
  std::vector<std::pair<MidiOutlet*, ossia::outlet_ptr>> m_midOutlets;
  JS::Script* m_object{};
  ossia::qt::qml_engine_functions* m_execFuncs{};
  std::optional<QJSValueList> m_tickCall;
  QPointer<QObject> m_uiContext;
  std::function<void(QVariant)> m_messageToUi;
  std::size_t m_gcIndex{};

  JS::JSState m_modelState;

  bool triggerStart{};
  bool triggerStop{};
  bool triggerPause{};
  bool triggerResume{};

  void setControl(std::size_t index, const QVariant& val)
  {
    if(index > m_jsInlets.size())
      return;
    if(auto v = qobject_cast<ValueInlet*>(m_jsInlets[index]))
      v->setValue(val);
    else if(auto v = qobject_cast<ControlInlet*>(m_jsInlets[index]))
      v->setValue(val);
  }
  void impulse(std::size_t index)
  {
    if(index > m_jsInlets.size())
      return;
    if(auto v = qobject_cast<Impulse*>(m_jsInlets[index]))
      v->impulse();
  }

  void uiMessage(const QVariant& v)
  {
    if(!m_object)
      return;

    const auto& on_ui = this->m_object->uiEvent();
    if(!on_ui.isCallable())
      return;

    on_ui.call({m_engine->toScriptValue(v)});
  }

  void stateElementChanged(const QString& k, const ossia::value& v)
  {
    if(!m_object)
      return;

    const auto& on_stateUpdated = this->m_object->stateUpdated();
    if(!on_stateUpdated.isCallable())
      return;

    if(v.valid())
    {
      if(auto res = v.apply(ossia::qt::ossia_to_qvariant{}); res.isValid())
        on_stateUpdated.call({k, m_engine->toScriptValue(res)});
      else
        on_stateUpdated.call({k, QJSValue{}});
    }
    else
      on_stateUpdated.call({k, QJSValue{}});
  }
};

struct js_process final : public ossia::node_process
{
  using node_process::node_process;
  js_node& js() const { return static_cast<js_node&>(*node); }
  void start() override { js().triggerStart = true; }
  void stop() override { js().triggerStop = true; }
  void pause() override { js().triggerPause = true; }
  void resume() override { js().triggerResume = true; }
  void transport_impl(ossia::time_value date) override
  {
    QMetaObject::invokeMethod(
        js().m_object, "transport", Qt::DirectConnection,
        Q_ARG(QVariant, double(date.impl)));
  }
  void offset_impl(ossia::time_value date) override
  {
    QMetaObject::invokeMethod(
        js().m_object, "offset", Qt::DirectConnection,
        Q_ARG(QVariant, double(date.impl)));
  }
};

}
