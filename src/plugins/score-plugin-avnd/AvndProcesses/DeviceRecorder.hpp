#pragma once
#include <State/Value.hpp>

#include <ossia/detail/parse_strict.hpp>
#include <ossia/network/value/detail/value_conversion_impl.hpp>

#include <QDateTime>
#include <QFile>

#include <AvndProcesses/AddressTools.hpp>
#include <AvndProcesses/Utils.hpp>
#include <csv2/csv2.hpp>
#include <halp/audio.hpp>

namespace avnd_tools
{
/** Records the input into a CSV.
 *  To record an entire device: can be a pattern expression such as foo://
 *  
 *  Writing to the disk is done in a worker thread as is tradition.
 */
struct DeviceRecorder : PatternObject
{
  halp_meta(name, "CSV recorder")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Recording")
  halp_meta(description, "Record the messages of a device at regular interval")
  halp_meta(c_name, "avnd_device_recorder")
  halp_meta(uuid, "7161ca22-5684-48f2-bde7-88933500a7fb")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/csv-recorder.html#csv-recorder")

  // Threaded worker
  struct recorder_thread
  {
    explicit recorder_thread(const score::DocumentContext& context)
        : context{context}
    {
    }
    const score::DocumentContext& context;
    QFile f{};
    std::string filename;
    std::vector<ossia::net::node_base*> roots;
    std::chrono::steady_clock::time_point first_ts;
    fmt::memory_buffer buf;
    bool active{};
    bool first_is_timestamp = false;
    int num_params = 0;

    void setActive(bool b)
    {
      active = b;
      if(!b)
        f.close();
      else
        reopen();
    }

    void reopen()
    {
      f.close();

      f.setFileName(filter_filename(this->filename, context));
      if(f.fileName().isEmpty())
        return;

      if(!active)
        return;

      f.open(QIODevice::WriteOnly);
      if(!f.isOpen())
        return;

      f.write("timestamp");
      num_params = 0;
      for(auto in : this->roots)
      {
        if(auto p = in->get_parameter())
        {
          f.write(",");
          f.write(QByteArray::fromStdString(p->get_node().osc_address()));

          num_params++;
        }
      }
      f.write("\n");
      f.flush();

      first_ts = std::chrono::steady_clock::now();
      buf.clear();
      buf.reserve(512);
    }

    void write()
    {
      if(!f.isOpen())
        return;

      using namespace std::chrono;
      const auto ts
          = duration_cast<milliseconds>(steady_clock::now() - first_ts).count();
      write(ts);
    }

    void write(int64_t timestamp)
    {
      f.write(QString::number(timestamp).toUtf8());
      for(auto in : this->roots)
      {
        if(auto p = in->get_parameter())
        {
          f.write(",");
          buf.clear();

          ossia::apply(ossia::detail::fmt_writer{buf}, p->value());

          std::string_view sv(buf.data(), buf.data() + buf.size());
          if(sv.find_first_of(", \"\n\r\t;") != std::string_view::npos)
          {
            // FIXME quote escaping
            f.write("\"", 1);
            f.write(buf.data(), buf.size());
            f.write("\"", 1);
          }
          else
          {
            f.write(buf.data(), buf.size());
          }
        }
      }
      f.write("\n");
      f.flush();
    }
  };

  struct player_thread
  {
    explicit player_thread(const score::DocumentContext& context)
        : context{context}
    {
    }
    const score::DocumentContext& context;
    QFile f{};
    std::string filename;
    std::vector<ossia::net::node_base*> roots;
    std::chrono::steady_clock::time_point first_ts;
    int64_t nots_index{};

    // FIXME boost::multi_array
    boost::container::flat_map<int, ossia::net::parameter_base*> m_map;
    boost::container::flat_map<int64_t, std::vector<ossia::value>> m_vec_ts;
    std::vector<std::vector<ossia::value>> m_vec_no_ts;
    bool active{};
    bool loops{};
    bool first_is_timestamp = false;
    int num_params{};

    void setActive(bool b)
    {
      active = b;
      if(!b)
        f.close();
      else
        reopen();
    }

    void setLoops(bool b) { loops = b; }
    void reopen()
    {
      f.close();

      f.setFileName(filter_filename(this->filename, context));
      if(f.fileName().isEmpty())
        return;

      if(!active)
        return;

      f.open(QIODevice::ReadOnly);
      if(!f.isOpen())
        return;
      if(f.size() <= 0)
        return;

      // FIXME not valid when the OSC device changes
      // We need to parse the header instead and have a map.
      num_params = 0;
      for(auto in : this->roots)
      {
        if([[maybe_unused]] auto p = in->get_parameter())
        {
          num_params++;
        }
      }

      auto data = (const char*)f.map(0, f.size());
      m_map.clear();

      csv2::Reader<> r;
      r.parse_view({data, data + f.size()});
      int columns = r.cols();

      auto header = r.header();

      boost::container::flat_map<std::string, ossia::net::parameter_base*> params;

      for(auto node : roots)
        if(auto p = node->get_parameter())
          params[node->osc_address()] = p;

      std::string v;
      v.reserve(128);
      int i = 0;
      auto header_it = header.begin();
      if(first_is_timestamp)
      {
        // this library increments upon dereference...
        // just doing ++header_it does not go to the next cell, but
        // to the next character so we have to do it just when skipping the ,

        *header_it;
        ++header_it;
      }

      for(; header_it != header.end(); ++header_it)
      {
        auto addr = *header_it;
        v.clear();
        addr.read_raw_value(v);
        if(auto it = params.find(v); it != params.end())
        {
          m_map[i] = it->second;
        }
        i++;
      }

      v.clear();
      m_vec_ts.clear();
      m_vec_no_ts.clear();
      if(first_is_timestamp)
      {
        csv2::Reader<> r;
        r.parse_view({data, data + f.size()});
        m_vec_ts.reserve(r.rows());
        for(const auto& row : r)
        {
          parse_row_with_timestamps(columns, row, v);
          v.clear();
        }
      }
      else
      {
        m_vec_no_ts.reserve(r.rows());
        for(const auto& row : r)
        {
          parse_row_no_timestamps(columns, row, v);
          v.clear();
        }
      }
      first_ts = std::chrono::steady_clock::now();
    }

    void parse_cell_impl(
        const std::string& v, ossia::net::parameter_base& param, ossia::value& out)
    {
      if(!v.empty())
      {
        std::optional<ossia::value> res;
        if(v.starts_with('"') && v.ends_with('"'))
          res = State::parseValue(std::string_view(v).substr(1, v.size() - 2));
        else
          res = State::parseValue(v);

        if(res)
        {
          out = std::move(*res);
          if(auto t = param.get_value_type(); out.get_type() != t)
          {
            ossia::convert(out, t);
          }
        }
      }
    }

    void
    parse_cell(const auto& cell, std::string& v, std::vector<ossia::value>& vec, int i)
    {
      if(auto param = m_map[i])
      {
        v.clear();
        cell.read_value(v);
        parse_cell_impl(v, *param, vec[i]);
        v.clear();
      }
    }

    void parse_row_no_timestamps(int columns, auto& row, std::string& v)
    {
      auto& vec = this->m_vec_no_ts.emplace_back(columns);
      int i = 0;

      for(auto it = row.begin(); it != row.end(); ++it)
      {
        parse_cell(*it, v, vec, i);
        i++;
      }
    }

    void parse_row_with_timestamps(int columns, auto& row, std::string& v)
    {
      if(row.length() <= 1)
        return;

      auto it = row.begin();
      const auto& ts = *it;

      v.clear();
      ts.read_value(v);
      auto tstamp = ossia::parse_strict<int64_t>(v);
      if(!tstamp)
        return;
      v.clear();
      auto& vec = this->m_vec_ts[*tstamp];
      vec.resize(columns - 1);
      int i = 0;

      for(++it; it != row.end(); ++it)
      {
        parse_cell(*it, v, vec, i);
        i++;
      }
    }

    void read()
    {
      if(first_is_timestamp)
      {
        if(m_vec_ts.empty())
          return;

        using namespace std::chrono;
        auto ts = duration_cast<milliseconds>(steady_clock::now() - first_ts).count();
        if(loops)
          ts %= m_vec_ts.rbegin()->first + 1;
        read_ts(ts);
      }
      else
      {
        if(m_vec_no_ts.empty())
          return;

        using namespace std::chrono;
        if(loops && nots_index >= std::ssize(m_vec_no_ts))
          nots_index = 0;
        read_no_ts(nots_index++);
      }
    }

    void read_no_ts(int64_t timestamp)
    {
      if(timestamp < 0)
        return;
      if(timestamp >= std::ssize(m_vec_no_ts))
        return;
      auto it = m_vec_no_ts.begin() + timestamp;
      if(it != m_vec_no_ts.end())
      {
        int i = 0;
        for(auto& v : *it)
        {
          if(v.valid())
          {
            if(auto p = m_map.find(i); p != m_map.end())
            {
              p->second->push_value(v);
            }
          }
          i++;
        }
      }
    }

    void read_ts(int64_t timestamp)
    {
      auto it = m_vec_ts.lower_bound(timestamp);
      if(it != m_vec_ts.end())
      {
        if(it != m_vec_ts.begin())
          --it;
        int i = 0;
        for(auto& v : it->second)
        {
          if(v.valid())
          {
            if(auto p = m_map.find(i); p != m_map.end())
            {
              p->second->push_value(v);
            }
          }
          i++;
        }
      }
      else
      {
        int i = 0;
        for(auto& v : m_vec_ts.rbegin()->second)
        {
          if(v.valid())
          {
            if(auto p = m_map.find(i); p != m_map.end())
            {
              p->second->push_value(v);
            }
          }
          i++;
        }
      }
    }
  };

  // Object definition
  struct inputs_t
  {
    PatternSelector pattern;
    halp::time_chooser<"Interval", halp::range{.min = 0.00001, .max = 5., .init = 0.25}>
        time;
    struct : halp::lineedit<"File pattern", "">
    {
      void update(DeviceRecorder& self) { self.update(); }
    } filename;
    struct
    {
      halp__enum("Mode", None, None, Record, Playback, Loop)
      void update(DeviceRecorder& self) { self.setMode(); }
    } mode;
    struct ts : halp::toggle<"Timestamped", halp::default_on_toggle>
    {
      halp_meta(description, "Set to true to use the first column as timestamp")
    } timestamped;
  } inputs;

  struct
  {
  } outputs;

  struct reset_message
  {
    std::shared_ptr<recorder_thread> recorder;
    std::shared_ptr<player_thread> player;
    std::string path;
    std::vector<ossia::net::node_base*> roots;
    bool first_is_timestamp{};

    void operator()()
    {
      using namespace std;
      swap(recorder->filename, path);
      swap(recorder->roots, roots);
      player->filename = recorder->filename;
      player->roots = recorder->roots;
      player->first_is_timestamp = first_is_timestamp;
      recorder->first_is_timestamp = first_is_timestamp;
      recorder->reopen();
      player->reopen();
    }
  };

  struct reset_path_message
  {
    std::shared_ptr<recorder_thread> recorder;
    std::shared_ptr<player_thread> player;
    std::string path;
    bool first_is_timestamp{};
    void operator()()
    {
      using namespace std;
      swap(recorder->filename, path);
      player->filename = recorder->filename;
      player->first_is_timestamp = first_is_timestamp;
      recorder->first_is_timestamp = first_is_timestamp;
      recorder->reopen();
      player->reopen();
    }
  };

  struct process_message
  {
    std::shared_ptr<recorder_thread> recorder;
    void operator()() { recorder->write(); }
  };

  struct playback_message
  {
    std::shared_ptr<player_thread> player;
    void operator()() { player->read(); }
  };

  struct activate_message
  {
    std::shared_ptr<recorder_thread> recorder;
    std::shared_ptr<player_thread> player;
    using mode_type = decltype(DeviceRecorder::inputs_t{}.mode.value);
    mode_type mode{};
    void operator()()
    {
      recorder->setActive(mode == mode_type::Record);
      player->setActive(mode == mode_type::Playback || mode == mode_type::Loop);
      player->setLoops(mode == mode_type::Loop);
    }
  };

  using worker_message = ossia::variant<
      std::unique_ptr<reset_message>, reset_path_message, process_message,
      playback_message, activate_message>;

  struct
  {
    std::function<void(worker_message)> request;
    static void work(worker_message&& mess)
    {
      ossia::visit([&]<typename M>(M&& msg) {
        if constexpr(requires { *msg; })
          (*std::forward<M>(msg))();
        else
          std::forward<M>(msg)();
      }, std::move(mess));
    }
  } worker;

  using tick = halp::tick_musical;

  void setMode()
  {
    if(!record_impl)
      return;
    worker.request(activate_message{record_impl, play_impl, inputs.mode.value});
  }

  void prepare()
  {
    SCORE_ASSERT(ossia_document_context);
    record_impl = std::make_shared<recorder_thread>(*ossia_document_context);
    play_impl = std::make_shared<player_thread>(*ossia_document_context);
  }

  void update()
  {
    if(!record_impl)
      return;
    worker.request(
        reset_path_message{record_impl, play_impl, inputs.filename, inputs.timestamped});
  }

  void operator()(const halp::tick_musical& tk)
  {
    int64_t elapsed_ns = 0.;
    if(!first_message_sent_pos)
      first_message_sent_pos = tk.position_in_nanoseconds;
    if(last_message_sent_pos)
      elapsed_ns = tk.position_in_nanoseconds - *last_message_sent_pos;

    if(elapsed_ns > 0 && elapsed_ns < inputs.time.value * 1e9)
      return;
    last_message_sent_pos = tk.position_in_nanoseconds;

    if(!m_path)
      return;

    if(!std::exchange(started, true))
    {
      inputs.pattern.reprocess();
      worker.request(std::unique_ptr<reset_message>(new reset_message{
          record_impl, play_impl, inputs.filename, roots, inputs.timestamped}));
    }

    switch(inputs.mode)
    {
      case decltype(inputs.mode)::None:
        break;
      case decltype(inputs.mode)::Record:
        worker.request(process_message{record_impl});
        break;
      case decltype(inputs.mode)::Playback:
      case decltype(inputs.mode)::Loop:
        worker.request(playback_message{play_impl});
        break;
    }
  }

  const score::DocumentContext* ossia_document_context{};
  std::shared_ptr<recorder_thread> record_impl;
  std::shared_ptr<player_thread> play_impl;
  std::optional<int64_t> first_message_sent_pos;
  std::optional<int64_t> last_message_sent_pos;
  bool started{};
};
}
