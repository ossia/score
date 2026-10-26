#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <Crousti/Concepts.hpp>
#include <Crousti/MessageBus.hpp>
#include <Crousti/Metadata.hpp>
#include <Crousti/Metadatas.hpp>
#include <Crousti/ProcessModelPortInit.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>

#include <score/serialization/MapSerialization.hpp>
#include <score/tools/std/HashMap.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/string_map.hpp>
#include <ossia/detail/type_if.hpp>
#include <ossia/detail/typelist.hpp>

#include <score/tools/File.hpp>

#include <QPointer>
#include <QFileInfo>
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
  //! The process box, the node of the nodal view and the inspector each show
  //! the process: every one of them gets the messages. A view that is gone is
  //! forgotten at the next message, or when another view is added.
  std::vector<std::pair<QPointer<QObject>, std::function<void(QByteArray)>>>
      to_ui_listeners;

  void add_to_ui_listener(QObject* view, std::function<void(QByteArray)> f)
  {
    std::erase_if(to_ui_listeners, [](const auto& l) { return l.first.isNull(); });
    to_ui_listeners.emplace_back(view, std::move(f));
  }

  std::function<void(QByteArray)> to_ui = [this](QByteArray mess) {
    std::erase_if(to_ui_listeners, [](const auto& l) { return l.first.isNull(); });
    const auto n = to_ui_listeners.size();
    for(std::size_t i = 0; i < n; i++)
    {
      // A listener that adds a view also drops the views gone meanwhile
      if(i >= to_ui_listeners.size())
        break;
      // The listener may add another view: hold a copy of its callback.
      auto f = to_ui_listeners[i].second;
      if(i + 1 == n)
        f(std::move(mess));
      else
        f(mess);
    }
  };

  MessageBusWrapperToUi() = default;
  MessageBusWrapperToUi(const MessageBusWrapperToUi&) = delete;
  MessageBusWrapperToUi& operator=(const MessageBusWrapperToUi&) = delete;
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
  ossia::type_if<Info, oscr::has_ports_callbacks<Info>>
      object_storage_for_ports_callbacks;

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
    upgrade_value_inlets_to_controls();
    upgrade_controls_to_value_inlets();
    upgrade_changed_control_types();
    init_after_port_creation();
  }

  ~ProcessModel() override { }

  //! A preset names each control it sets besides giving its id: an entry
  //! whose id is not a control of that name finds the control by its name, or
  //! by the name it had (`former_name()`), so that a preset outlives controls
  //! added, removed or moved. A preset saved before presets named their
  //! controls gives ids only: an object that has removed inlets since lists
  //! the ids they had then in `removed_inlets()`, and the ids of such a preset
  //! are counted without them.
  //! A value saved when a control was another kind of control goes through the
  //! control's `upgrade_value`, as the value of a document saved then does
  //! (see upgrade_changed_control_types), and a choice saved by the name it
  //! had through `former_values()`.
  void loadPreset(const Process::Preset& preset) override
  {
    const rapidjson::Document doc = readJson(preset.data);
    if(!doc.IsArray())
      return;
    for(const auto& arr : doc.GetArray())
    {
      if(!arr.IsArray() || arr.Size() < 2 || !arr[0].IsInt())
        continue;
      Process::ControlInlet* ctrl{};
      if(arr.Size() >= 3 && arr[2].IsString())
        ctrl = preset_control(
            arr[0].GetInt(),
            QString::fromUtf8(arr[2].GetString(), qsizetype(arr[2].GetStringLength())));
      else
        ctrl = unnamed_preset_control(arr[0].GetInt());
      if(!ctrl)
        continue;
      ossia::value val = JsonValue{arr[1]}.to<ossia::value>();
      upgrade_preset_value(*ctrl, val);
      ctrl->setValue(val);
    }
  }

  //! [id, value, name] for each control: see loadPreset.
  Process::Preset savePreset() const noexcept override
  {
    Process::Preset p;
    p.name = this->metadata().getName();
    p.key.key = this->concreteKey();

    JSONReader r;
    r.stream.StartArray();
    for(const auto& inlet : m_inlets)
    {
      if(auto ctrl = qobject_cast<Process::ControlInlet*>(inlet))
      {
        r.stream.StartArray();
        r.stream.Int(ctrl->id().val());
        r.readFrom(ctrl->value());
        const QByteArray name = ctrl->name().toUtf8();
        r.stream.String(name.data(), rapidjson::SizeType(name.size()));
        r.stream.EndArray();
      }
    }
    r.stream.EndArray();
    p.data = r.toByteArray();
    return p;
  }

private:
  Process::ControlInlet* control_named(const QString& name, int id) const noexcept
  {
    Process::ControlInlet* res{};
    for(auto* in : m_inlets)
    {
      if(in->name() != name)
        continue;
      if(auto ctrl = qobject_cast<Process::ControlInlet*>(in))
      {
        if(ctrl->id().val() == id)
          return ctrl;
        if(!res)
          res = ctrl;
      }
    }
    return res;
  }

  Process::ControlInlet* preset_control(int id, const QString& name) const noexcept
  {
    if(auto ctrl = control_named(name, id))
      return ctrl;
    Process::ControlInlet* res{};
    avnd::input_introspection<Info>::for_all(
        [&]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(requires { P::former_name(); } && requires { P::name(); })
      {
        const auto was = P::former_name();
        if(res || name != QString::fromUtf8(was.data(), qsizetype(was.size())))
          return;
        const auto now = P::name();
        res = control_named(QString::fromUtf8(now.data(), qsizetype(now.size())), id);
      }
    });
    return res;
  }

  Process::ControlInlet* unnamed_preset_control(int id) const noexcept
  {
    if constexpr(requires { Info::removed_inlets(); })
    {
      // The ports of the process are where the spec declares them, whatever
      // their ids in this document.
      static constexpr auto removed = Info::removed_inlets();
      int position = id;
      for(int r : removed)
      {
        if(r == id)
          return nullptr;
        if(r < id)
          position--;
      }
      if(position < 0 || position >= std::ssize(m_inlets))
        return nullptr;
      return qobject_cast<Process::ControlInlet*>(m_inlets[position]);
    }
    else
    {
      return qobject_cast<Process::ControlInlet*>(
          this->inlet(Id<Process::Port>{id}));
    }
  }

  //! The choice a saved name of a choice is now, if it is one of the names in
  //! `former_values()`.
  template <typename P>
  static std::optional<std::string> current_choice(const std::string& saved)
  {
    static constexpr auto range = avnd::get_range<P>();
    const auto names = avnd::to_enum_range(range.values);
    if(ossia::any_of(names, [&](const auto& n) { return n == saved; }))
      return std::nullopt;
    const auto former = P::former_values();
    for(std::size_t i = 0; i < former.size() && i < names.size(); i++)
      if(former[i] == saved)
        return std::string(names[i]);
    return std::nullopt;
  }

  void upgrade_preset_value(const Process::ControlInlet& ctrl, ossia::value& val)
  {
    avnd::input_introspection<Info>::for_all(
        [&]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(requires { P::former_values(); })
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() == 1 && ports[0] == &ctrl)
          if(const auto* str = val.target<std::string>())
            if(auto now = current_choice<P>(*str))
              val = std::move(*now);
      }
    });
    if(val.get_type() == ctrl.value().get_type())
      return;
    avnd::input_introspection<Info>::for_all(
        [&]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(requires(const ossia::value& v) { P::upgrade_value(v); })
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() == 1 && ports[0] == &ctrl)
          val = P::upgrade_value(std::as_const(val));
      }
      else if constexpr(requires(float f) {
                          { P::upgrade_value(f) } -> std::convertible_to<float>;
                        })
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() == 1 && ports[0] == &ctrl && is_number(val))
          if(auto v = carried_value(
                 float(P::upgrade_value(ossia::convert<float>(val))), ctrl))
            val = std::move(*v);
      }
    });
  }

  void init_common()
  {
    if constexpr(avnd::tag_loops_by_default<Info>)
      setLoops(true);
  }

  void init_before_port_creation() { init_dynamic_ports(); }
  void init_after_port_creation()
  {
    init_controller_ports();
    init_folder_comboboxes();
    init_single_cable_ports();
  }

  void init_single_cable_ports()
  {
    avnd::input_introspection<Info>::for_all(
        [this]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(single_cable_port<P>)
        for(auto* port : avnd_input_idx_to_model_ports(Idx))
          port->singleCable = true;
    });
  }

  // Folder-backed comboboxes (halp::folder_combobox): each carries the name of
  // a sibling folder port; list that folder's files as the combobox items, at
  // edit/load time, and refresh whenever the folder value changes. Runs after
  // deserialization, so a loaded document's folder value is already in place.
  void init_folder_comboboxes()
  {
    for(auto* inl : m_inlets)
    {
      auto combo = qobject_cast<Process::ComboBox*>(inl);
      if(!combo || combo->folderPortName.isEmpty())
        continue;

      Process::ControlInlet* folderPort = nullptr;
      for(auto* other : m_inlets)
      {
        auto ci = qobject_cast<Process::ControlInlet*>(other);
        if(ci && ci != combo && ci->name() == combo->folderPortName)
        {
          folderPort = ci;
          break;
        }
      }

      auto pathOf = [this](Process::ControlInlet* p) -> QString {
        if(!p)
          return {};
        if(auto s = p->value().target<std::string>())
        {
          // Stored paths are templates (<PROJECT>:..., <LIBRARY>:..., or
          // document-relative); resolve before touching the filesystem, or a
          // project-relative folder lists nothing.
          auto path = QString::fromStdString(*s);
          if(path.isEmpty())
            return {};
          // Walk up rather than score::IDocument::documentFromObject, which
          // throws when there is no document -- ports are built before the
          // process is attached to one.
          score::Document* doc{};
          for(QObject* o = this->parent(); o && !doc; o = o->parent())
            doc = qobject_cast<score::Document*>(o);
          if(doc)
            path = score::locateFilePath(path, doc->context());
          // The sibling port may name a folder, or a file (e.g. a sound-file
          // port): in the latter case list the file's containing folder.
          if(QFileInfo fi{path}; fi.isFile())
            return fi.absolutePath();
          return path;
        }
        return {};
      };

      combo->repopulateFromFolder(pathOf(folderPort));
      if(folderPort)
        QObject::connect(
            folderPort, &Process::ControlInlet::valueChanged, combo,
            [combo, folderPort, pathOf](const ossia::value&) {
          combo->repopulateFromFolder(pathOf(folderPort));
        });
    }
  }
  void check_all_ports()
  {
    if(std::ssize(m_inlets) == expected_input_ports()
       && std::ssize(m_outlets) == expected_output_ports())
    {
      rename_former_kept_ports();
      upgrade_former_values();
      return;
    }

    std::vector<Dataflow::SavedPort> oldInlets, oldOutlets;
    std::vector<saved_port> oldInletInfo, oldOutletInfo;
    for(auto& port : m_inlets)
    {
      oldInlets.emplace_back(
          Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
      oldInletInfo.push_back(saved_port::of(*port));
    }
    for(auto& port : m_outlets)
    {
      oldOutlets.emplace_back(
          Dataflow::SavedPort{port->name(), port->type(), port->saveData()});
      oldOutletInfo.push_back(saved_port::of(*port));
    }

    qDeleteAll(m_inlets);
    m_inlets.clear();
    qDeleteAll(m_outlets);
    m_outlets.clear();

    init_all_ports();
    init_dynamic_port_instances();

    rename_former_ports(oldInlets);

    // A port the spec gained since the save just comes back at its default:
    // only a saved port that matched nothing actually lost its value.
    std::vector<int> inletMatches, outletMatches;
    const auto lost = Dataflow::reloadPortsInNewProcess(
        oldInlets, oldOutlets, *this, &inletMatches, &outletMatches);
    keep_saved_ids(m_inlets, inletMatches, oldInletInfo);
    keep_saved_ids(m_outlets, outletMatches, oldOutletInfo);
    upgrade_rebuilt_control_kinds(inletMatches, oldInletInfo);
    upgrade_former_values();
    if(!lost.empty())
    {
      const std::string_view type_name = avnd::get_name<Info>();
      qWarning() << QLatin1String(type_name.data(), type_name.size())
                 << this->metadata().getName()
                 << ": saved ports lost on load (renamed or removed since):"
                 << lost.join(", ");
    }
  }

  //! What check_all_ports keeps of a saved port besides its data.
  struct saved_port
  {
    int id{};
    UuidKey<Process::Port> key;
    //! A control's value.
    std::optional<ossia::value> value;
    bool stable{};

    static saved_port of(const Process::Port& port)
    {
      saved_port p{port.id().val(), port.concreteKey(), std::nullopt, port.stableIdentity};
      if(auto ctrl = qobject_cast<const Process::ControlInlet*>(&port))
        p.value = ctrl->value();
      return p;
    }
  };

  //! Ports rebuilt from a document keep the ids they were saved with: the
  //! document's cables find their ends by those ids. A port the document did
  //! not have takes an id that none of them uses.
  template <typename Ports>
  static void keep_saved_ids(
      Ports& ports, const std::vector<int>& matches, const std::vector<saved_port>& saved)
  {
    if(matches.size() != ports.size())
      return;
    std::vector<int> ids(ports.size());
    std::vector<int> taken;
    int next = 0;
    for(std::size_t i = 0; i < ports.size(); i++)
    {
      next = std::max(next, ports[i]->id().val() + 1);
      ids[i] = ports[i]->id().val();
      if(matches[i] >= 0 && !ports[i]->stableIdentity && !saved[matches[i]].stable)
        ids[i] = saved[matches[i]].id;
      if(matches[i] >= 0 || ports[i]->stableIdentity)
        taken.push_back(ids[i]);
    }
    for(const auto& p : saved)
      next = std::max(next, p.id + 1);
    for(std::size_t i = 0; i < ports.size(); i++)
    {
      if(matches[i] < 0 && !ports[i]->stableIdentity)
      {
        if(ossia::contains(taken, ids[i]))
          ids[i] = next++;
        taken.push_back(ids[i]);
      }
      if(ports[i]->id().val() != ids[i])
        ports[i]->setId(Id<Process::Port>{ids[i]});
    }
  }

  //! A control rebuilt from a document that saved it as another kind of
  //! control gets the value the kept-ports path gives it, see
  //! upgrade_changed_control_types.
  void upgrade_rebuilt_control_kinds(
      const std::vector<int>& matches, const std::vector<saved_port>& saved)
  {
    avnd::input_introspection<Info>::for_all(
        [&]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(
          avnd::parameter_port<P> && !oscr::ossia_port<P>
          && !avnd::dynamic_ports_port<P> && avnd::has_widget<P>
          && !requires { P::on_controller_interaction(); })
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() != 1)
          return;
        const auto pos = std::size_t(ossia::find(m_inlets, ports[0]) - m_inlets.begin());
        if(pos >= matches.size() || matches[pos] < 0)
          return;
        const auto& old = saved[std::size_t(matches[pos])];
        auto fresh = qobject_cast<Process::ControlInlet*>(ports[0]);
        if(!fresh || !old.value || old.key == fresh->concreteKey())
          return;
        if(auto v = upgraded_value<P>(*old.value, *fresh))
          fresh->setValue(*v);
      }
    });
  }

  //! A control renamed since a document was saved names what it was called,
  //! `former_name()`: the saved port of that name is the control's, unless
  //! one of its new name was saved as well. That one is the control's unless
  //! its name was another control's, renamed as well: then the document is
  //! from before both, and each saved port goes to the control it was.
  void rename_former_ports(std::vector<Dataflow::SavedPort>& saved)
  {
    struct rename
    {
      QString was, now;
      bool applies{};
    };
    std::vector<rename> renames;
    avnd::input_introspection<Info>::for_all(
        [&]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(requires { P::former_name(); } && requires { P::name(); })
      {
        const auto now = P::name();
        const auto was = P::former_name();
        renames.push_back(
            {QString::fromUtf8(was.data(), qsizetype(was.size())),
             QString::fromUtf8(now.data(), qsizetype(now.size()))});
      }
    });
    const auto was_saved = [&](const QString& name) {
      return ossia::any_of(saved, [&](const auto& p) { return p.name == name; });
    };
    for(bool changed = true; changed;)
    {
      changed = false;
      for(auto& r : renames)
      {
        if(r.applies || !was_saved(r.was))
          continue;
        const bool taken_by_now = was_saved(r.now)
                                  && !ossia::any_of(renames, [&](const rename& o) {
          return o.applies && o.was == r.now;
        });
        if(!taken_by_now)
          changed = r.applies = true;
      }
    }
    // All at once, from the names as they were saved.
    std::vector<std::pair<std::size_t, QString>> moves;
    for(const auto& r : renames)
    {
      if(!r.applies)
        continue;
      for(std::size_t i = 0; i < saved.size(); i++)
        if(saved[i].name == r.was
           && !ossia::any_of(moves, [i](const auto& m) { return m.first == i; }))
        {
          moves.emplace_back(i, r.now);
          break;
        }
    }
    for(auto& [i, name] : moves)
      saved[i].name = name;
  }

  //! A control renamed since a document was saved whose ports are all there:
  //! the port that has the name it had, `former_name()`, takes its new one.
  void rename_former_kept_ports()
  {
    avnd::input_introspection<Info>::for_all(
        [this]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(requires { P::former_name(); } && requires { P::name(); })
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() != 1)
          return;
        const auto was = P::former_name();
        if(ports[0]->name() != QString::fromUtf8(was.data(), qsizetype(was.size())))
          return;
        const auto now = P::name();
        ports[0]->setName(QString::fromUtf8(now.data(), qsizetype(now.size())));
      }
    });
  }

  //! A choice renamed since a document was saved: `former_values()` lists the
  //! names the choices had, in their order, and a saved one is the choice at
  //! its place.
  void upgrade_former_values()
  {
    avnd::input_introspection<Info>::for_all(
        [this]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(requires { P::former_values(); })
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() != 1)
          return;
        auto inlet = qobject_cast<Process::ControlInlet*>(ports[0]);
        if(!inlet)
          return;
        const ossia::value& value = inlet->value();
        const auto* str = value.template target<std::string>();
        if(!str)
          return;
        if(auto now = current_choice<P>(*str))
          inlet->setValue(std::move(*now));
      }
    });
  }

  //! A document saved before the controllers had a port type of their own has
  //! the controller as the plain control it derives from (e.g. a
  //! Process::IntSpinBox), which is what the port factory of that key loads.
  //! It would work, but with the generic widget and command: the ones that
  //! edit the value from inside the widget's own event, and lose the cables on
  //! undo. Swap it for the port the process would create today, same id,
  //! same data.
  template <typename F, std::size_t Idx>
  Process::ControlInlet* upgradeControllerPort(F& field, avnd::field_index<Idx> idx)
  {
    auto ports = avnd_input_idx_to_model_ports(Idx);
    SCORE_ASSERT(ports.size() == 1);
    auto old_inlet = qobject_cast<Process::ControlInlet*>(ports[0]);
    SCORE_ASSERT(old_inlet);

    Process::Inlets fresh;
    InletInitFunc<Info> make{*this, fresh};
    make.inlet = old_inlet->id().val();
    make(field, idx);
    if(fresh.size() != 1)
    {
      qDeleteAll(fresh);
      return old_inlet;
    }

    auto new_inlet = qobject_cast<Process::ControlInlet*>(fresh[0]);
    if(!new_inlet || new_inlet->concreteKey() == old_inlet->concreteKey())
    {
      qDeleteAll(fresh);
      return old_inlet;
    }

    new_inlet->loadData(old_inlet->saveData(), Process::PortLoadDataFlags::ReloadValue);
    new_inlet->setExposed(old_inlet->exposed());
    new_inlet->setDescription(old_inlet->description());
    new_inlet->displayHandledExplicitly = old_inlet->displayHandledExplicitly;

    auto it = ossia::find(m_inlets, old_inlet);
    SCORE_ASSERT(it != m_inlets.end());
    *it = new_inlet;
    delete old_inlet;
    return new_inlet;
  }

  //! What a control saved as another kind keeps of its value: the same type
  //! of value as is, a number into a number or a time chooser (in seconds, as
  //! the execution reads a plain number). A field whose unit changed as well
  //! (milliseconds, Hz...) converts it with `static float upgrade_value(float
  //! old)`, or `static ossia::value upgrade_value(const ossia::value& old)`.
  static bool is_number(const ossia::value& v) noexcept
  {
    const auto t = v.get_type();
    return t == ossia::val_type::FLOAT || t == ossia::val_type::INT
           || t == ossia::val_type::BOOL;
  }

  static std::optional<ossia::value>
  carried_value(const ossia::value& old, const Process::ControlInlet& fresh)
  {
    const auto from = old.get_type();
    const auto to = fresh.value().get_type();
    if(from == to)
      return old;
    if(!is_number(old))
      return std::nullopt;
    if(qobject_cast<const Process::TimeChooser*>(&fresh))
      return ossia::vec2f{ossia::convert<float>(old), 0.f};
    switch(to)
    {
      case ossia::val_type::FLOAT:
        return ossia::convert<float>(old);
      case ossia::val_type::INT:
        return int(std::lround(ossia::convert<float>(old)));
      case ossia::val_type::BOOL:
        return ossia::convert<bool>(old);
      default:
        return std::nullopt;
    }
  }

  //! What the control P, of another kind than it was saved as, keeps of the
  //! value it was saved with.
  template <typename P>
  static std::optional<ossia::value>
  upgraded_value(const ossia::value& old_value, const Process::ControlInlet& fresh)
  {
    if constexpr(requires(float f) {
                   { P::upgrade_value(f) } -> std::convertible_to<float>;
                 })
    {
      if(!is_number(old_value))
        return std::nullopt;
      return carried_value(float(P::upgrade_value(ossia::convert<float>(old_value))), fresh);
    }
    else if constexpr(requires { P::upgrade_value(old_value); })
      return P::upgrade_value(old_value);
    else
      return carried_value(old_value, fresh);
  }

  //! A control saved as another kind of control than the spec now declares
  //! (e.g. a maintained button that became an impulse button, a float slider
  //! that became a time chooser) is rebuilt as the declared kind, with the
  //! same id -- cables and addresses keep pointing at it -- and its cable
  //! list, exposed name, address and value (see carried_value). The rest
  //! (range, init, description) is the spec's.
  void upgrade_changed_control_types()
  {
    avnd::input_introspection<Info>::for_all(
        [this]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(
          avnd::parameter_port<P> && !oscr::ossia_port<P>
          && !avnd::dynamic_ports_port<P> && avnd::has_widget<P>
          && std::is_default_constructible_v<P>
          // Controllers have their own upgrade, which keeps their value
          // (init_controller_ports).
          && !requires { P::on_controller_interaction(); })
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() != 1)
          return;
        auto old_inlet = qobject_cast<Process::ControlInlet*>(ports[0]);
        if(!old_inlet)
          return;

        Process::Inlets fresh;
        InletInitFunc<Info> make{*this, fresh};
        make.inlet = old_inlet->id().val();
        make(P{}, avnd::field_index<Idx>{});
        auto new_inlet = fresh.size() == 1
                             ? qobject_cast<Process::ControlInlet*>(fresh[0])
                             : nullptr;
        if(!new_inlet || new_inlet->concreteKey() == old_inlet->concreteKey())
        {
          qDeleteAll(fresh);
          return;
        }

        new_inlet->setExposed(old_inlet->exposed());
        new_inlet->setAddress(old_inlet->address());
        if(auto v = upgraded_value<P>(old_inlet->value(), *new_inlet))
          new_inlet->setValue(*v);
        // The cables into it: the document's cables point at this id, and the
        // port lists them too.
        new_inlet->takeCables(std::move(*old_inlet));
        auto it = ossia::find(m_inlets, old_inlet);
        SCORE_ASSERT(it != m_inlets.end());
        *it = new_inlet;
        delete old_inlet;
      }
    });
  }

  //! The other way: a control that became a plain value port (e.g. Value
  //! delay's In, a float slider that now takes any value). Rebuilt as the
  //! declared port with the same id, cables, address and exposed name.
  void upgrade_controls_to_value_inlets()
  {
    avnd::input_introspection<Info>::for_all(
        [this]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(
          avnd::parameter_port<P> && !oscr::ossia_port<P>
          && !avnd::dynamic_ports_port<P> && !avnd::has_widget<P>
          && std::is_default_constructible_v<P>)
      {
        auto ports = avnd_input_idx_to_model_ports(Idx);
        if(ports.size() != 1)
          return;
        auto old_inlet = qobject_cast<Process::ControlInlet*>(ports[0]);
        if(!old_inlet)
          return;

        Process::Inlets fresh;
        InletInitFunc<Info> make{*this, fresh};
        make.inlet = old_inlet->id().val();
        make(P{}, avnd::field_index<Idx>{});
        auto new_inlet
            = fresh.size() == 1 ? qobject_cast<Process::ValueInlet*>(fresh[0]) : nullptr;
        if(!new_inlet)
        {
          qDeleteAll(fresh);
          return;
        }

        new_inlet->setExposed(old_inlet->exposed());
        new_inlet->setAddress(old_inlet->address());
        new_inlet->takeCables(std::move(*old_inlet));
        auto it = ossia::find(m_inlets, old_inlet);
        SCORE_ASSERT(it != m_inlets.end());
        *it = new_inlet;
        delete old_inlet;
      }
    });
  }

  void upgrade_value_inlets_to_controls()
  {
    avnd::input_introspection<Info>::for_all(
        [this]<std::size_t Idx, typename P>(avnd::field_reflection<Idx, P>) {
      if constexpr(
          avnd::parameter_port<P> && !oscr::ossia_port<P>
          && !avnd::dynamic_ports_port<P> && avnd::has_widget<P>
          && std::is_default_constructible_v<P>)
        upgrade_value_inlet(P{}, avnd::field_index<Idx>{});
    });
  }

  template <typename F, std::size_t Idx>
  void upgrade_value_inlet(const F& field, avnd::field_index<Idx> idx)
  {
    auto ports = avnd_input_idx_to_model_ports(Idx);
    if(ports.size() != 1)
      return;
    auto old_inlet = ports[0];
    if(qobject_cast<Process::ControlInlet*>(old_inlet)
       || !qobject_cast<Process::ValueInlet*>(old_inlet))
      return;

    Process::Inlets fresh;
    InletInitFunc<Info> make{*this, fresh};
    make.inlet = old_inlet->id().val();
    make(field, idx);
    auto new_inlet
        = fresh.size() == 1 ? qobject_cast<Process::ControlInlet*>(fresh[0]) : nullptr;
    if(!new_inlet)
    {
      qDeleteAll(fresh);
      return;
    }

    new_inlet->loadData(old_inlet->saveData());
    new_inlet->setExposed(old_inlet->exposed());

    auto it = ossia::find(m_inlets, old_inlet);
    SCORE_ASSERT(it != m_inlets.end());
    *it = new_inlet;
    delete old_inlet;
  }

  void init_controller_ports()
  {
    if constexpr(oscr::has_ports_callbacks<Info>)
    {
      avnd::control_input_introspection<Info>::for_all_n2(
          avnd::get_inputs<Info>((Info&)this->object_storage_for_ports_callbacks),
          [this]<std::size_t Idx, typename F>(
              F& field, auto pred_index, avnd::field_index<Idx> idx) {
        Info& obj = this->object_storage_for_ports_callbacks;
        if constexpr(requires { F::on_controller_interaction(); })
        {
          upgradeControllerPort(field, idx);
          // Version migration for controls whose old document representation
          // differs from their current value type (e.g. newline keywords).
          if constexpr(requires(ossia::value v) { F::migrate_value(v); })
          {
            auto inlet = qobject_cast<Process::ControlInlet*>(
                avnd_input_idx_to_model_ports(Idx)[0]);
            inlet->setValue(F::migrate_value(inlet->value()));
          }
        }
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
          inlet->changesPorts = true;

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
          this->request_new_dynamic_input_count(port, avnd::field_index<N>{}, new_count);
        };
        if constexpr(requires { port.request_port_rows; })
          port.request_port_rows = [this, &port](const auto& rows) {
            this->template request_dynamic_rows<true>(
                port, avnd::field_index<N>{}, rows);
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
          this->request_new_dynamic_output_count(
              port, avnd::field_index<N>{}, new_count);
        };
        if constexpr(requires { port.request_port_rows; })
          port.request_port_rows = [this, &port](const auto& rows) {
            this->template request_dynamic_rows<false>(
                port, avnd::field_index<N>{}, rows);
          };
      });
    }
  }

  template <bool Input, typename P, std::size_t N, typename Rows>
  void request_dynamic_rows(P& port, avnd::field_index<N> idx, const Rows& rows)
  {
    if(rows.size() > 512)
      return;
    std::vector<int> keys;
    keys.reserve(rows.size());
    for(const auto& [key, text] : rows)
    {
      if(key < 10000 || ossia::contains(keys, key))
        return;
      keys.push_back(key);
    }
    auto& all = [&]() -> auto& {
      if constexpr(Input)
        return m_inlets;
      else
        return m_outlets;
    }();
    auto first = [&] {
      if constexpr(Input)
        return avnd_input_idx_to_iterator(idx);
      else
        return avnd_output_idx_to_iterator(idx);
    }();
    auto& count = [&]() -> auto& {
      if constexpr(Input)
        return dynamic_ports.num_in_ports(idx);
      else
        return dynamic_ports.num_out_ports(idx);
    }();
    using Ports = std::decay_t<decltype(all)>;
    using Port = std::remove_pointer_t<typename Ports::value_type>;
    Ports old{first, first + count}, next;
    next.reserve(rows.size());
    // A row without text is named like a port of a count-based group: the
    // port's name with "{}" replaced by its position ("Input 2").
    using port_type = std::decay_t<decltype(port.ports[0])>;
    const auto templateName = [] {
      const std::string_view nm = avnd::get_name<port_type>();
      return QString::fromUtf8(nm.data(), nm.size());
    }();
    int row = 0;
    for(const auto& [key, text] : rows)
    {
      auto found
          = ossia::find_if(old, [key](auto p) { return p && p->id().val() == key; });
      Port* p{};
      if(found != old.end())
      {
        p = *found;
        *found = nullptr;
      }
      else
      {
        Ports added;
        if constexpr(Input)
        {
          InletInitFunc<Info> make{*this, added};
          make.inlet = key;
          make(port, idx);
        }
        else
        {
          OutletInitFunc<Info> make{*this, added};
          make.outlet = key;
          make(port, idx);
        }
        SCORE_ASSERT(added.size() == 1);
        p = added.front();
      }
      p->stableIdentity = true;
      auto name = QString::fromStdString(text);
      if(name.trimmed().isEmpty())
      {
        name = templateName;
        name.replace(QStringLiteral("{}"), QString::number(row));
      }
      p->setName(name);
      next.push_back(p);
      row++;
    }
    const bool changed = !std::equal(next.begin(), next.end(), first, first + count);
    const auto offset = first - all.begin();
    all.erase(first, first + count);
    all.insert(all.begin() + offset, next.begin(), next.end());
    count = rows.size();
    ossia::small_pod_vector<Port*, 4> removed;
    for(auto p : old)
      if(p)
        removed.push_back(p);
    removeCablesOfPorts(removed);
    if(changed)
    {
      if constexpr(Input)
        inletsChanged();
      else
        outletsChanged();
    }
    for(auto p : removed)
      delete p;
  }

  //! Ports removed by a resize take their cables with them: a cable whose end
  //! no longer exists is a dangling path in the document. When the resize
  //! comes from SetControllerControlValue the cables were already saved and
  //! removed by the command (and are restored by it), so this finds none.
  template <typename Port_T>
  void removeCablesOfPorts(const ossia::small_pod_vector<Port_T*, 4>& ports)
  {
    if(ports.empty())
      return;

    // Not score::IDocument::documentFromObject: it throws when there is no
    // document, and a process can be resized outside of one.
    score::Document* doc{};
    for(QObject* o = this->parent(); o && !doc; o = o->parent())
      doc = qobject_cast<score::Document*>(o);
    if(!doc)
      return;

    QObjectList objs;
    objs.reserve(ports.size());
    for(auto p : ports)
      objs.push_back(p);

    const auto& ctx = doc->context();
    if(auto cables = Dataflow::saveCables(objs, ctx); !cables.empty())
      Dataflow::removeCables(cables, ctx);
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

    removeCablesOfPorts(to_delete);
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

    removeCablesOfPorts(to_delete);
    outletsChanged();
    for(auto port : to_delete)
      delete port;
  }

  //! init_all_ports() builds the static ports only: re-create the dynamic
  //! ones at the counts read from the document, so that the saved ports find
  //! their place again and the counts match m_inlets / m_outlets.
  void init_dynamic_port_instances()
  {
    if constexpr(avnd::dynamic_ports_input_introspection<Info>::size > 0)
    {
      avnd::dynamic_ports_input_introspection<Info>::for_all_n2(
          avnd::get_inputs((Info&)object_storage_for_ports_callbacks),
          [&]<std::size_t N>(auto& port, auto, avnd::field_index<N> idx) {
        auto& count = dynamic_ports.num_in_ports(idx);
        const int n = std::exchange(count, 0);
        request_new_dynamic_input_count(port, idx, n);
      });
    }
    if constexpr(avnd::dynamic_ports_output_introspection<Info>::size > 0)
    {
      avnd::dynamic_ports_output_introspection<Info>::for_all_n2(
          avnd::get_outputs((Info&)object_storage_for_ports_callbacks),
          [&]<std::size_t N>(auto& port, auto, avnd::field_index<N> idx) {
        auto& count = dynamic_ports.num_out_ports(idx);
        const int n = std::exchange(count, 0);
        request_new_dynamic_output_count(port, idx, n);
      });
    }
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
    // (same adjustment as avnd_input_idx_to_model_ports)
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;
    else if constexpr(avnd::tag_cv<Info>)
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
    // (same adjustment as avnd_output_idx_to_model_ports: no message outlets,
    // and a cv processor only has an output if it returns something)
    if constexpr(avnd::audio_argument_processor<Info>)
      model_index += 1;
    else if constexpr(avnd::tag_cv<Info>)
    {
      using operator_ret = typename avnd::function_reflection_o<Info>::return_type;
      if constexpr(!std::is_void_v<operator_ret>)
        model_index += 1;
    }

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
