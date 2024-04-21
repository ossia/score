#pragma once
#include <ossia/network/value/detail/value_conversion_impl.hpp>

#include <QDateTime>
#include <QFile>

#include <AvndProcesses/AddressTools.hpp>
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

  // Threaded worker
  struct recorder_thread
  {
    QFile f{};
    std::string filename;
    std::vector<ossia::net::node_base*> roots;
    std::chrono::steady_clock::time_point first_ts;
    fmt::memory_buffer buf;

    void reopen()
    {
      f.close();

      auto filename = QByteArray::fromStdString(this->filename);
      filename.replace("%t", QDateTime::currentDateTimeUtc().toString().toUtf8());
      f.setFileName(filename);
      if(filename.isEmpty())
        return;

      f.open(QIODevice::WriteOnly);
      if(!f.isOpen())
        return;

      f.write("timestamp");
      for(auto in : this->roots)
      {
        if(auto p = in->get_parameter())
        {
          f.write(",");
          f.write(QByteArray::fromStdString(p->get_node().osc_address()));
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
      const auto ts = duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - first_ts)
                          .count();
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
          f.write(buf.data(), buf.size());
        }
      }
      f.write("\n");
      f.flush();
    }
  };
  std::shared_ptr<recorder_thread> impl = std::make_shared<recorder_thread>();

  struct reset_message
  {
    std::string path;
    std::vector<ossia::net::node_base*> roots;
    void operator()(recorder_thread& self)
    {
      using namespace std;
      swap(self.filename, path);
      swap(self.roots, roots);
      self.reopen();
    }
  };

  struct reset_path_message
  {
    std::string path;
    void operator()(recorder_thread& self)
    {
      using namespace std;
      swap(self.filename, path);
      self.reopen();
    }
  };

  struct process_message
  {
    void operator()(recorder_thread& self) { self.write(); }
  };
  using worker_message
      = ossia::variant<reset_message, reset_path_message, process_message>;

  struct
  {
    std::function<void(std::shared_ptr<recorder_thread>, worker_message)> request;
    static void work(std::shared_ptr<recorder_thread> t, worker_message&& mess)
    {
      ossia::visit(
          [&]<typename M>(M&& msg) { std::forward<M>(msg)(*t); }, std::move(mess));
    }
  } worker;

  // Object definition
  struct
  {
    PatternSelector pattern;
    halp::time_chooser<"Interval"> time;
    struct : halp::lineedit<"File pattern", "">
    {
      void update(DeviceRecorder& self)
      {
        self.worker.request(self.impl, reset_path_message{self.inputs.filename});
      }
    } filename;
  } inputs;

  struct
  {
  } outputs;

  using tick = halp::tick_musical;

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
      worker.request(impl, reset_message{inputs.filename, roots});
    }

    worker.request(impl, process_message{});
  }

  bool started{};
  std::optional<int64_t> first_message_sent_pos;
  std::optional<int64_t> last_message_sent_pos;
};

}
