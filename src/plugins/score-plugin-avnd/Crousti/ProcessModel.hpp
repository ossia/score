#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <Crousti/Attributes.hpp>
#include <Crousti/CodeWriter.hpp>
#include <Crousti/Concepts.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/Metadata.hpp>
#include <Crousti/Metadatas.hpp>
#include <Crousti/ProcessModelPortInit.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>

#include <score/serialization/MapSerialization.hpp>
#include <score/tools/std/HashMap.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/string_map.hpp>
#include <ossia/detail/type_if.hpp>
#include <ossia/detail/typelist.hpp>
#include <ossia/network/value/format_value.hpp>

#include <boost/pfr.hpp>

#include <QTimer>

#include <avnd/binding/ossia/data_node.hpp>
#include <avnd/binding/ossia/dynamic_ports.hpp>
#include <avnd/common/for_nth.hpp>
#include <avnd/concepts/gfx.hpp>
#include <avnd/concepts/ui.hpp>
#include <avnd/introspection/messages.hpp>
#include <avnd/wrappers/bus_host_process_adapter.hpp>

#include <score_plugin_engine.hpp>

/**
 * This file instantiates the classes that are provided by this plug-in.
 */

namespace oscr
{

template <typename Info>
struct MessageBusWrapperToUi
{
};

template <typename Info>
struct MessageBusWrapperFromUi
{
};

namespace
{
struct dummy_ui_callback
{
  void operator()(const QByteArray& arr) noexcept { }
};
}

template <avnd::has_processor_to_gui_bus Info>
struct MessageBusWrapperToUi<Info>
{
  std::function<void(QByteArray)> to_ui = dummy_ui_callback{};
};

template <avnd::has_gui_to_processor_bus Info>
struct MessageBusWrapperFromUi<Info>
{
  std::function<void(QByteArray)> from_ui = dummy_ui_callback{};
};

inline void hideAllInlets(Process::ProcessModel& proc)
{
  for(auto& p : proc.inlets())
    p->displayHandledExplicitly = true;
  for(auto& p : proc.outlets())
    p->displayHandledExplicitly = true;
}

template <typename Info>
class ProcessModel final
    : public Process::ProcessModel
    , public MessageBusWrapperFromUi<Info>
    , public MessageBusWrapperToUi<Info>
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(ProcessModel<Info>)
  friend struct TSerializer<DataStream, oscr::ProcessModel<Info>>;
  friend struct TSerializer<JSONObject, oscr::ProcessModel<Info>>;

public:
  [[no_unique_address]]
  oscr::dynamic_ports_storage<Info> dynamic_ports;

  [[no_unique_address]]
  ossia::type_if<Info, oscr::has_dynamic_ports<Info>> object_storage_for_ports_callbacks;

  ProcessModel(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx, QObject* parent)
      : Process::ProcessModel{
            duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    init_common();
    init_before_port_creation();
    init_all_ports();
    init_after_port_creation();
  }

  ProcessModel(
      const TimeVal& duration, const QString& custom,
      const Id<Process::ProcessModel>& id, QObject* parent)
      : Process::ProcessModel{
            duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  {
    metadata().setInstanceName(*this);

    init_common();
    init_before_port_creation();
    init_all_ports();

    if constexpr(avnd::file_input_introspection<Info>::size > 0)
    {
      static constexpr auto idx
          = avnd::file_input_introspection<Info>::index_to_field_index(0);
      setupInitialStringPort(idx, custom);
    }
    else if constexpr(avnd::control_input_introspection<Info>::size > 0)
    {
      static constexpr auto idx
          = avnd::control_input_introspection<Info>::index_to_field_index(0);
      using type =
          typename avnd::control_input_introspection<Info>::template nth_element<0>;
      if constexpr(avnd::string_ish<decltype(type::value)>)
        setupInitialStringPort(idx, custom);
    }
    init_after_port_creation();
  }

  void setupInitialStringPort(int idx, const QString& custom) noexcept
  {
    Process::Inlet* port = avnd_input_idx_to_model_ports(idx)[0];
    auto pp = safe_cast<Process::ControlInlet*>(port);

    if(auto val = pp->value(); bool(val.target<std::string>()))
    {
      pp->setValue(custom.toStdString());
    }
  }

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    init_common();
    init_before_port_creation();
    vis.writeTo(*this);
    check_all_ports();
    init_after_port_creation();
  }

  ~ProcessModel() override { }

private:
  void init_common()
  {
    if constexpr(avnd::tag_loops_by_default<Info>)
      setLoops(true);
  }

  void init_before_port_creation() { init_dynamic_ports(); }
  void init_after_port_creation() { init_controller_ports(); }
  void check_all_ports()
  {
    if(std::ssize(m_inlets) != expected_input_ports()
       || std::ssize(m_outlets) != expected_output_ports())
    {
      qDebug() << typeid(Info).name() << this->metadata().getName()
               << ": WARNING : process does not match spec: I " << m_inlets.size()
               << "but expected: " << expected_input_ports() << " ; O "
               << m_outlets.size() << "but expected: " << expected_output_ports();

      std::vector<Dataflow::SavedPort> m_oldInlets, m_oldOutlets;
      for(auto& port : m_inlets)
        m_oldInlets.emplace_back(
            Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
      for(auto& port : m_outlets)
        m_oldOutlets.emplace_back(
            Dataflow::SavedPort{port->name(), port->type(), port->saveData()});

      qDeleteAll(m_inlets);
      m_inlets.clear();
      qDeleteAll(m_outlets);
      m_outlets.clear();

      init_all_ports();

      Dataflow::reloadPortsInNewProcess(m_oldInlets, m_oldOutlets, *this);
    }
  }

  void init_controller_ports()
  {
    if constexpr(
        avnd::dynamic_ports_input_introspection<Info>::size > 0
        || avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      avnd::control_input_introspection<Info>::for_all_n2(
          avnd::get_inputs<Info>((Info&)this->object_storage_for_ports_callbacks),
          [this]<std::size_t Idx, typename F>(
              F& field, auto pred_index, avnd::field_index<Idx>) {
        Info& obj = this->object_storage_for_ports_callbacks;
        if constexpr(requires { F::on_controller_setup(); })
        {
          auto controller_inlets = avnd_input_idx_to_model_ports(Idx);
          SCORE_ASSERT(controller_inlets.size() == 1);
          auto inlet = qobject_cast<Process::ControlInlet*>(controller_inlets[0]);

          oscr::from_ossia_value(inlet->value(), field.value);

          if_possible(field.update(obj));

          F::on_controller_setup()(obj, field.value);
        }
        if constexpr(requires { F::on_controller_interaction(); })
        {
          auto controller_inlets = avnd_input_idx_to_model_ports(Idx);
          SCORE_ASSERT(controller_inlets.size() == 1);
          auto inlet = qobject_cast<Process::ControlInlet*>(controller_inlets[0]);
          inlet->noValueChangeOnMove = true;

          if constexpr(!requires { F::on_controller_setup(); })
          {
            oscr::from_ossia_value(inlet->value(), field.value);
            if_possible(field.update(obj));
            F::on_controller_interaction()(obj, field.value);
          }

          connect(
              inlet, &Process::ControlInlet::valueChanged,
              [this, &field](const ossia::value& val) {
            Info& obj = this->object_storage_for_ports_callbacks;
            oscr::from_ossia_value(val, field.value);

            if_possible(field.update(obj));

            F::on_controller_interaction()(obj, field.value);
          });
        }
      });

      if constexpr(avnd::has_gui_to_processor_bus<Info>)
      {
        // FIXME needs to be a list of callbacks?
      }

      if constexpr(avnd::has_processor_to_gui_bus<Info>)
      {
        Info& obj = this->object_storage_for_ports_callbacks;

        obj.send_message = [this]<typename T>(T&& b) mutable {
          if(this->to_ui)
            MessageBusSender{this->to_ui}(std::move(b));
        };
      }
    }
  }

  void init_dynamic_ports()
  {
    // To check:

    // - serialization
    // - OK: uses of total_input_count / total_output_count

    // - OK: uses of oscr::modelPort (btw this does not seem to match total_input_count)

    // - execution, resize the avnd object with the number of ports
    //  -> connect controls so that they change in the ui

    // - OK: Layer::createControl: uses index_in_struct and assumes it's a ProcessModel::inlet index

    // How are things going to happen UI-wise: all controls created one after each other ?
    // what if we want to create separate tabs with e.g.
    // control A 1, control B 1
    // control A 2, control B 2
    // we need to know the current state of dynamic ports

    // UI: recreating UI causes crash when editing e.g. context menu of spinbox
    // UI: recreating UI causes bug when editing e.g. spinbox

    // -> we have to find a way to detect that the currently edited object is being deleted

    // other option: mark widget as "unsafe", do not have it send value changes until mouse is released

    // REloading on crash: does crash, because the control change does not have
    // the time to be applied. In DocumentBuilder::loadCommandStack we have
    // to process the event loop in-between events.
    // FIxed if we put zero but then changing the widget with the mouses crashes because of the comment
    // below...
    // ->
    //   terminate called after throwing an instance of 'std::runtime_error'
    //   what():  Ongoing command mismatch: current command SetControlValue does not match new command MoveNodes

    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      Info& obj = object_storage_for_ports_callbacks;
      avnd::dynamic_ports_input_introspection<Info>::for_all_n2(
          avnd::get_inputs(obj),
          [&]<std::size_t N>(auto& port, auto pred_idx, avnd::field_index<N> field_idx) {
        port.request_port_resize = [this, &port](int new_count) {
          // We're in the ui thread, we can just push the request directly.
          // With some delay as otherwise we may be deleting the widget we
          // are clicking on before mouse release and Qt really doesn't like that
          // But when we are loading the document we actually do not want the
          // delay to make sure the port is created before the cable connects to it
          if(score::IDocument::documentFromObject(*this)->loaded())
          {
            QTimer::singleShot(0, this, [self = QPointer{this}, &port, new_count] {
              if(self)
                self->request_new_dynamic_input_count(
                    port, avnd::field_index<N>{}, new_count);
            });
          }
          else
          {
            this->request_new_dynamic_input_count(
                port, avnd::field_index<N>{}, new_count);
          }
        };
      });
    }

    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      Info& obj = object_storage_for_ports_callbacks;
      avnd::dynamic_ports_output_introspection<Info>::for_all_n2(
          avnd::get_outputs(obj),
          [&]<std::size_t N>(auto& port, auto pred_idx, avnd::field_index<N> field_idx) {
        port.request_port_resize = [this, &port](int new_count) {
          // See comment above for inputs
          if(score::IDocument::documentFromObject(*this)->loaded())
          {
            QTimer::singleShot(0, this, [self = QPointer{this}, &port, new_count] {
              if(self)
                self->request_new_dynamic_output_count(
                    port, avnd::field_index<N>{}, new_count);
            });
          }
          else
          {
            this->request_new_dynamic_output_count(
                port, avnd::field_index<N>{}, new_count);
          }
        };
      });
    }
  }

  template <typename P, std::size_t N>
  void request_new_dynamic_input_count(P& port, avnd::field_index<N> idx, int count)
  {
    const int current_model_ports = dynamic_ports.num_in_ports(idx);
    if(current_model_ports == count || count < 0 || count > 512)
      return;

    ossia::small_pod_vector<Process::Inlet*, 4> to_delete;
    if(current_model_ports < count)
    {
      // Add new ports
      // 1. Find the location where to add them
      auto res = avnd_input_idx_to_iterator(idx);
      res += current_model_ports;
      Process::Inlets inlets_to_add;
      InletInitFunc<Info> inlets{*this, inlets_to_add};
      inlets.inlet = 10000 + N * 1000 + current_model_ports;
      int sz = 0;
      for(int i = current_model_ports; i < count; i++)
      {
        inlets(port, idx);
        if(std::ssize(inlets_to_add) > sz)
        {
          sz = std::ssize(inlets_to_add);
          auto new_inlet = inlets_to_add.back();
          if(auto nm = new_inlet->name(); nm.contains("{}"))
          {
            nm.replace("{}", QString::number(i));
            new_inlet->setName(nm);
          }
        }
      }
      m_inlets.insert(res, inlets_to_add.begin(), inlets_to_add.end());
    }
    else if(current_model_ports > count)
    {
      // Delete the ports
      auto res = avnd_input_idx_to_iterator(idx);
      res += count;
      auto begin_deleted = res;
      for(int i = 0; i < (current_model_ports - count); i++)
      {
        to_delete.push_back(*res);
        ++res;
      }
      m_inlets.erase(begin_deleted, res);
    }

    dynamic_ports.num_in_ports(idx) = count;

    inletsChanged();
    for(auto port : to_delete)
      delete port;
  }

  template <typename P, std::size_t N>
  void request_new_dynamic_output_count(P& port, avnd::field_index<N> idx, int count)
  {
    const int current_model_ports = dynamic_ports.num_out_ports(idx);
    if(current_model_ports == count || count < 0 || count > 512)
      return;

    ossia::small_pod_vector<Process::Outlet*, 4> to_delete;
    if(current_model_ports < count)
    {
      // Add new ports
      // 1. Find the location where to add them
      auto res = avnd_output_idx_to_iterator(idx);
      res += current_model_ports;
      Process::Outlets outlets_to_add;
      OutletInitFunc<Info> outlets{*this, outlets_to_add};
      outlets.outlet = 1000000 + N * 1000 + current_model_ports;
      int sz = 0;
      for(int i = current_model_ports; i < count; i++)
      {
        outlets(port, idx);
        if(std::ssize(outlets_to_add) > sz)
        {
          sz = std::ssize(outlets_to_add);
          auto new_outlet = outlets_to_add.back();
          if(auto nm = new_outlet->name(); nm.contains("{}"))
          {
            nm.replace("{}", QString::number(i));
            new_outlet->setName(nm);
          }
        }
      }
      m_outlets.insert(res, outlets_to_add.begin(), outlets_to_add.end());
    }
    else if(current_model_ports > count)
    {
      // Delete the ports
      auto res = avnd_output_idx_to_iterator(idx);
      res += count;
      auto begin_deleted = res;
      for(int i = 0; i < (current_model_ports - count); i++)
      {
        to_delete.push_back(*res);
        ++res;
      }
      m_outlets.erase(begin_deleted, res);
    }

    dynamic_ports.num_out_ports(idx) = count;

    outletsChanged();
    for(auto port : to_delete)
      delete port;
  }

  void init_all_ports()
  {
    InletInitFunc<Info> inlets{*this, m_inlets};
    OutletInitFunc<Info> outlets{*this, m_outlets};
    avnd::port_visit_dispatcher<Info>([&inlets]<typename P>(P&& port, auto idx) {
      if constexpr(!avnd::dynamic_ports_port<P>)
        inlets(port, idx);
    }, [&outlets]<typename P>(P&& port, auto idx) {
      if constexpr(!avnd::dynamic_ports_port<P>)
        outlets(port, idx);
    });

    if(!requires { Info::ossia_show_ports_by_default; })
      if constexpr(oscr::has_ossia_layer<Info>)
        hideAllInlets(*this);
  }

public:
  int expected_input_ports() const noexcept
  {
    int count = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      count += 1;
    else if constexpr(avnd::tag_cv<Info>)
      count += 1;

    // The "messages" ports also go before
    count += avnd::messages_introspection<Info>::size;

    avnd::input_introspection<Info>::for_all([this, &count]<std::size_t Idx, typename P>(
                                                 avnd::field_reflection<Idx, P> field) {
      int num_ports = 1;
      if constexpr(avnd::dynamic_ports_port<P>)
        num_ports = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
      count += num_ports;
    });

    return count;
  }

  int expected_output_ports() const noexcept
  {
    int count = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      count += 1;
    else if constexpr(avnd::tag_cv<Info>)
    {
      using operator_ret = typename avnd::function_reflection_o<Info>::return_type;
      if constexpr(!std::is_void_v<operator_ret>)
        count += 1;
    }

    avnd::output_introspection<Info>::for_all(
        [this,
         &count]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
      int num_ports = 1;
      if constexpr(avnd::dynamic_ports_port<P>)
        num_ports = dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
      count += num_ports;
    });

    return count;
  }

  std::span<Process::Inlet*> avnd_input_idx_to_model_ports(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;
    else if constexpr(avnd::tag_cv<Info>)
      model_index += 1;

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    std::span<Process::Inlet*> ret;
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size == 0)
    {
      ret = std::span<Process::Inlet*>(
          const_cast<Process::Inlet**>(this->m_inlets.data()) + model_index + index, 1);
    }
    else
    {
      avnd::input_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          int num_ports = 1;
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            num_ports = dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
            if(num_ports == 0)
            {
              ret = {};
              return;
            }
          }
          ret = std::span<Process::Inlet*>(
              const_cast<Process::Inlet**>(this->m_inlets.data()) + model_index,
              num_ports);
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }

    return ret;
  }

  std::span<Process::Outlet*> avnd_output_idx_to_model_ports(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;
    else if constexpr(avnd::tag_cv<Info>)
    {
      using operator_ret = typename avnd::function_reflection_o<Info>::return_type;
      if constexpr(!std::is_void_v<operator_ret>)
        model_index += 1;
    }

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    std::span<Process::Outlet*> ret;
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size == 0)
    {
      ret = std::span<Process::Outlet*>(
          const_cast<Process::Outlet**>(this->m_outlets.data()) + model_index + index,
          1);
    }
    else
    {
      avnd::output_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          int num_ports = 1;
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            num_ports = dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
            if(num_ports == 0)
            {
              ret = {};
              return;
            }
          }
          ret = std::span<Process::Outlet*>(
              const_cast<Process::Outlet**>(this->m_outlets.data()) + model_index,
              num_ports);
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }

    return ret;
  }

  Process::Inlets::iterator avnd_input_idx_to_iterator(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    Process::Inlets::iterator ret;
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size == 0)
    {
      ret = const_cast<ProcessModel*>(this)->m_inlets.begin() + model_index;
    }
    else
    {
      avnd::input_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          ret = const_cast<ProcessModel*>(this)->m_inlets.begin() + model_index;
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }
    return ret;
  }

  Process::Outlets::iterator avnd_output_idx_to_iterator(int index) const noexcept
  {
    int model_index = 0;

    // We have to adjust before accessing a port as there is the first "fake"
    // port if the processor takes audio by argument
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;

    // The "messages" ports also go before
    model_index += avnd::messages_introspection<Info>::size;

    Process::Outlets::iterator ret;
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size == 0)
    {
      ret = const_cast<ProcessModel*>(this)->m_outlets.begin() + model_index;
    }
    else
    {
      avnd::output_introspection<Info>::for_all(
          [this, index, &model_index,
           &ret]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if(Idx == index)
        {
          ret = const_cast<ProcessModel*>(this)->m_outlets.begin() + model_index;
        }
        else
        {
          if constexpr(avnd::dynamic_ports_port<P>)
          {
            model_index += dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
          }
          else
          {
            model_index += 1;
          }
        }
      });
    }
    return ret;
  }

  std::unique_ptr<Process::CodeWriter>
  codeWriter(Process::CodeFormat) const noexcept override
  {
    if constexpr(requires { new Info::code_writer{*this}; })
      return std::make_unique<typename Info::code_writer>(*this);
    else
      return std::make_unique<Crousti::CodeWriter<Info>>(*this);
  };
};
}

template <typename Info>
struct is_custom_serialized<oscr::ProcessModel<Info>> : std::true_type
{
};

template <typename Info>
struct TSerializer<DataStream, oscr::ProcessModel<Info>>
{
  using model_type = oscr::ProcessModel<Info>;
  static void readFrom(DataStream::Serializer& s, const model_type& obj)
  {
    Process::readPorts(s, obj.m_inlets, obj.m_outlets);

    // Save the recorded amount of dynamic ports for each port
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      avnd::dynamic_ports_input_introspection<Info>::for_all(
          [&obj, &s]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if constexpr(avnd::dynamic_ports_port<P>)
          s.stream() << obj.dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
      });
    }
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      avnd::dynamic_ports_output_introspection<Info>::for_all(
          [&obj, &s]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if constexpr(avnd::dynamic_ports_port<P>)
          s.stream() << obj.dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
      });
    }

    s.insertDelimiter();
  }

  static void writeTo(DataStream::Deserializer& s, model_type& obj)
  {
    Process::writePorts(
        s, s.components.interfaces<Process::PortFactoryList>(), obj.m_inlets,
        obj.m_outlets, &obj);

    // Read the recorded amount of dynamic ports for each port
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      avnd::dynamic_ports_input_introspection<Info>::for_all(
          [&obj, &s]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if constexpr(avnd::dynamic_ports_port<P>)
          s.stream() >> obj.dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
      });
    }
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      avnd::dynamic_ports_output_introspection<Info>::for_all(
          [&obj, &s]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if constexpr(avnd::dynamic_ports_port<P>)
          s.stream() >> obj.dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
      });
    }
    s.checkDelimiter();
  }
};

template <typename Info>
struct TSerializer<JSONObject, oscr::ProcessModel<Info>>
{
  using model_type = oscr::ProcessModel<Info>;
  static void readFrom(JSONObject::Serializer& s, const model_type& obj)
  {
    Process::readPorts(s, obj.m_inlets, obj.m_outlets);
    // Save the recorded amount of dynamic ports for each port
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      ossia::string_map<int> indices;
      avnd::dynamic_ports_input_introspection<Info>::for_all(
          [&obj,
           &indices]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if constexpr(avnd::dynamic_ports_port<P>)
        {
          indices[std::string(avnd::get_c_identifier<P>())]
              = obj.dynamic_ports.num_in_ports(avnd::field_index<Idx>{});
        }
      });
      s.obj["DynamicInlets"] = indices;
    }
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      ossia::string_map<int> indices;
      avnd::dynamic_ports_output_introspection<Info>::for_all(
          [&obj,
           &indices]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
        if constexpr(avnd::dynamic_ports_port<P>)
          indices[std::string(avnd::get_c_identifier<P>())]
              = obj.dynamic_ports.num_out_ports(avnd::field_index<Idx>{});
      });
      s.obj["DynamicOutlets"] = indices;
    }
  }

  static void writeTo(JSONObject::Deserializer& s, model_type& obj)
  {
    Process::writePorts(
        s, s.components.interfaces<Process::PortFactoryList>(), obj.m_inlets,
        obj.m_outlets, &obj);
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      if(auto val = s.obj.tryGet("DynamicInlets"))
      {
        static ossia::string_map<int> indices;
        indices.clear();
        indices <<= *val;
        avnd::dynamic_ports_input_introspection<Info>::for_all(
            [&obj]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
          if constexpr(avnd::dynamic_ports_port<P>)
            obj.dynamic_ports.num_in_ports(avnd::field_index<Idx>{})
                = indices[std::string(avnd::get_c_identifier<P>())];
        });
      }
    }
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      if(auto val = s.obj.tryGet("DynamicOutlets"))
      {
        static ossia::string_map<int> indices;
        indices.clear();
        indices <<= *val;
        avnd::dynamic_ports_output_introspection<Info>::for_all(
            [&obj]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P> field) {
          if constexpr(avnd::dynamic_ports_port<P>)
            obj.dynamic_ports.num_out_ports(avnd::field_index<Idx>{})
                = indices[std::string(avnd::get_c_identifier<P>())];
        });
      }
    }
  }
};
