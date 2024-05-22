#pragma once
#include <ossia/network/value/detail/value_conversion_impl.hpp>

#include <QDateTime>
#include <QFile>

#include <AvndProcesses/AddressTools.hpp>
#include <halp/audio.hpp>
/**
 csv2: https://github.com/p-ranav/csv2/
 MIT License

Copyright (c) 2019 Pranav

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

namespace csv2
{
namespace trim_policy
{
struct no_trimming
{
public:
  static std::pair<size_t, size_t> trim(const char* buffer, size_t start, size_t end)
  {
    (void)(buffer); // to silence unused parameter warning
    return {start, end};
  }
};

template <char... character_list>
struct trim_characters
{
private:
  constexpr static bool is_trim_char(char) { return false; }

  template <class... Tail>
  constexpr static bool is_trim_char(char c, char head, Tail... tail)
  {
    return c == head || is_trim_char(c, tail...);
  }

public:
  static std::pair<size_t, size_t> trim(const char* buffer, size_t start, size_t end)
  {
    size_t new_start = start, new_end = end;
    while(new_start != new_end && is_trim_char(buffer[new_start], character_list...))
      ++new_start;
    while(new_start != new_end && is_trim_char(buffer[new_end - 1], character_list...))
      --new_end;
    return {new_start, new_end};
  }
};

using trim_whitespace = trim_characters<' ', '\t'>;
} // namespace trim_policy

template <char character>
struct delimiter
{
  constexpr static char value = character;
};

template <char character>
struct quote_character
{
  constexpr static char value = character;
};

template <bool flag>
struct first_row_is_header
{
  constexpr static bool value = flag;
};

template <
    class delimiter = delimiter<','>, class quote_character = quote_character<'"'>,
    class first_row_is_header = first_row_is_header<true>,
    class trim_policy = trim_policy::trim_whitespace>
class Reader
{
  const char* buffer_{nullptr}; // pointer to memory-mapped data
  size_t buffer_size_{0};       // mapped length of buffer
  size_t header_start_{0};      // start index of header (cache)
  size_t header_end_{0};        // end index of header (cache)

public:
  bool parse_view(std::string_view sv)
  {
    buffer_ = sv.data();
    buffer_size_ = sv.size();
    return buffer_size_ > 0;
  }

  class RowIterator;
  class Row;
  class CellIterator;

  class Cell
  {
    const char* buffer_{nullptr}; // Pointer to memory-mapped buffer
    size_t start_{0};             // Start index of cell content
    size_t end_{0};               // End index of cell content
    bool escaped_{false};         // Does the cell have escaped content?
    friend class Row;
    friend class CellIterator;

  public:
    // returns a view on the cell's contents if C++17 available
    std::string_view read_view() const
    {
      const auto new_start_end = trim_policy::trim(buffer_, start_, end_);
      return std::string_view(
          buffer_ + new_start_end.first, new_start_end.second - new_start_end.first);
    }
    // Returns the raw_value of the cell without handling escaped
    // content, e.g., cell containing """foo""" will be returned
    // as is
    template <typename Container>
    void read_raw_value(Container& result) const
    {
      if(start_ >= end_)
        return;
      result.reserve(end_ - start_);
      for(size_t i = start_; i < end_; ++i)
        result.push_back(buffer_[i]);
    }

    // If cell is escaped, convert and return correct cell contents,
    // e.g., """foo""" => ""foo""
    template <typename Container>
    void read_value(Container& result) const
    {
      if(start_ >= end_)
        return;
      result.reserve(end_ - start_);
      const auto new_start_end = trim_policy::trim(buffer_, start_, end_);
      for(size_t i = new_start_end.first; i < new_start_end.second; ++i)
        result.push_back(buffer_[i]);
      for(size_t i = 1; i < result.size(); ++i)
      {
        if(result[i] == quote_character::value
           && result[i - 1] == quote_character::value)
        {
          result.erase(i - 1, 1);
        }
      }
    }
  };

  class Row
  {
    const char* buffer_{nullptr}; // Pointer to memory-mapped buffer
    size_t start_{0};             // Start index of row content
    size_t end_{0};               // End index of row content
    friend class RowIterator;
    friend class Reader;

  public:
    // address of row
    const char* address() const { return buffer_; }
    // returns the char length of the row
    size_t length() const { return end_ - start_; }

    // Returns the raw_value of the row
    template <typename Container>
    void read_raw_value(Container& result) const
    {
      if(start_ >= end_)
        return;
      result.reserve(end_ - start_);
      for(size_t i = start_; i < end_; ++i)
        result.push_back(buffer_[i]);
    }

    class CellIterator
    {
      friend class Row;
      const char* buffer_;
      size_t buffer_size_;
      size_t start_;
      size_t current_;
      size_t end_;

    public:
      CellIterator(const char* buffer, size_t buffer_size, size_t start, size_t end)
          : buffer_(buffer)
          , buffer_size_(buffer_size)
          , start_(start)
          , current_(start_)
          , end_(end)
      {
      }

      CellIterator& operator++()
      {
        current_ += 1;
        return *this;
      }

      Cell operator*()
      {
        bool escaped{false};
        class Cell cell;
        cell.buffer_ = buffer_;
        cell.start_ = current_;
        cell.end_ = end_;

        size_t last_quote_location = 0;
        bool quote_opened = false;
        for(auto i = current_; i < end_; i++)
        {
          current_ = i;
          if(buffer_[i] == delimiter::value && !quote_opened)
          {
            // actual delimiter
            // end of cell
            cell.end_ = current_;
            cell.escaped_ = escaped;
            return cell;
          }
          else
          {
            if(buffer_[i] == quote_character::value)
            {
              if(!quote_opened)
              {
                // first quote for this cell
                quote_opened = true;
                last_quote_location = i;
              }
              else
              {
                escaped = (last_quote_location == i - 1);
                last_quote_location += (i - last_quote_location) * size_t(!escaped);
                quote_opened = escaped || (buffer_[i + 1] != delimiter::value);
              }
            }
          }
        }
        cell.end_ = current_ + 1;
        return cell;
      }

      bool operator!=(const CellIterator& rhs) { return current_ != rhs.current_; }
    };

    CellIterator begin() const
    {
      return CellIterator(buffer_, end_ - start_, start_, end_);
    }
    CellIterator end() const { return CellIterator(buffer_, end_ - start_, end_, end_); }
  };

  class RowIterator
  {
    friend class Reader;
    const char* buffer_;
    size_t buffer_size_;
    size_t start_;
    size_t end_;

  public:
    RowIterator(const char* buffer, size_t buffer_size, size_t start)
        : buffer_(buffer)
        , buffer_size_(buffer_size)
        , start_(start)
        , end_(start_)
    {
    }

    RowIterator& operator++()
    {
      start_ = end_ + 1;
      end_ = start_;
      return *this;
    }

    Row operator*()
    {
      Row result;
      result.buffer_ = buffer_;
      result.start_ = start_;
      result.end_ = end_;

      if(const char* ptr = static_cast<const char*>(
             memchr(&buffer_[start_], '\n', (buffer_size_ - start_))))
      {
        end_ = start_ + (ptr - &buffer_[start_]);
        result.end_ = end_;
        start_ = end_ + 1;
      }
      else
      {
        // last row
        end_ = buffer_size_;
        result.end_ = end_;
      }
      return result;
    }

    bool operator!=(const RowIterator& rhs) { return start_ != rhs.start_; }
  };

  RowIterator begin() const
  {
    if(buffer_size_ == 0)
      return end();
    if(first_row_is_header::value)
    {
      const auto header_indices = header_indices_();
      return RowIterator(
          buffer_, buffer_size_,
          header_indices.second > 0 ? header_indices.second + 1 : 0);
    }
    else
    {
      return RowIterator(buffer_, buffer_size_, 0);
    }
  }

  RowIterator end() const
  {
    return RowIterator(buffer_, buffer_size_, buffer_size_ + 1);
  }

private:
  std::pair<size_t, size_t> header_indices_() const
  {
    size_t start = 0, end = 0;

    if(const char* ptr
       = static_cast<const char*>(memchr(&buffer_[start], '\n', (buffer_size_ - start))))
    {
      end = start + (ptr - &buffer_[start]);
    }
    return {start, end};
  }

public:
  Row header() const
  {
    size_t start = 0, end = 0;
    Row result;
    result.buffer_ = buffer_;
    result.start_ = start;
    result.end_ = end;

    if(const char* ptr
       = static_cast<const char*>(memchr(&buffer_[start], '\n', (buffer_size_ - start))))
    {
      end = start + (ptr - &buffer_[start]);
      result.end_ = end;
    }
    return result;
  }

  /**
   * @returns The number of rows (excluding the header)
  */
  size_t rows(bool ignore_empty_lines = false) const
  {
    size_t result{0};
    if(!buffer_ || buffer_size_ == 0)
      return result;

    // Count the first row if not header
    if(not first_row_is_header::value
       and (not ignore_empty_lines or *(static_cast<const char*>(buffer_)) != '\r'))
      ++result;

    for(const char* p = buffer_;
        (p = static_cast<const char*>(memchr(p, '\n', (buffer_ + buffer_size_) - p)));
        ++p)
    {
      if(ignore_empty_lines and (p >= buffer_ + buffer_size_ - 1 or *(p + 1) == '\r'))
        continue;
      ++result;
    }
    return result;
  }

  size_t cols() const
  {
    size_t result{0};
    for(const auto cell : header())
      result += 1;
    return result;
  }
};
}

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
    bool active{};
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

      auto filename = QByteArray::fromStdString(this->filename);
      filename.replace("%t", QDateTime::currentDateTimeUtc().toString().toUtf8());
      f.setFileName(filename);
      if(filename.isEmpty())
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
    QFile f{};
    std::string filename;
    std::vector<ossia::net::node_base*> roots;
    std::chrono::steady_clock::time_point first_ts;
    boost::container::flat_map<int, ossia::net::parameter_base*> m_map;
    boost::container::flat_map<int64_t, std::vector<ossia::value>> m_vec;
    bool active{};
    int num_params{};

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

      auto filename = QByteArray::fromStdString(this->filename);
      filename.replace("%t", QDateTime::currentDateTimeUtc().toString().toUtf8());
      f.setFileName(filename);
      if(filename.isEmpty())
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
        if(auto p = in->get_parameter())
        {
          num_params++;
        }
      }

      auto data = (const char*)f.map(0, f.size());
      m_vec.clear();
      m_map.clear();

      csv2::Reader<> r;
      r.parse_view({data, data + f.size()});
      m_vec.reserve(r.rows());
      int columns = r.cols();

      auto header = r.header();

      boost::container::flat_map<std::string, ossia::net::parameter_base*> params;

      for(auto node : roots)
        if(auto p = node->get_parameter())
          params[node->osc_address()] = p;

      int i = 0;
      for(csv2::Reader<>::Row::CellIterator header_it = ++header.begin();
          header_it != header.end(); ++header_it)
      {
        auto addr = *header_it;
        std::string a;
        addr.read_raw_value(a);
        if(auto it = params.find(a); it != params.end())
        {
          m_map[i - 1] = it->second;
        }
        i++;
      }

      std::string v;
      v.reserve(128);
      for(const auto& row : r)
      {
        if(row.length() > 1)
        {
          auto it = row.begin();
          const auto& ts = *it;

          ts.read_value(v);
          int64_t tstamp;
          const auto [ptr, ec] = std::from_chars(v.data(), v.data() + v.size(), tstamp);

          if(ec == std::errc{} && ptr == v.data() + v.size())
          {
            auto& vec = this->m_vec[tstamp];
            vec.resize(columns - 1);
            int i = 0;

            for(++it; it != row.end(); ++it)
            {
              const auto& cell = *it;
              v.clear();
              cell.read_value(v);
              auto res = State::parseValue(v);
              if(res)
              {
                vec[i] = std::move(*res);
              }
              v.clear();
              i++;
            }
          }
        }
        v.clear();
      }
      first_ts = std::chrono::steady_clock::now();
    }

    void read()
    {
      using namespace std::chrono;
      const auto ts
          = duration_cast<milliseconds>(steady_clock::now() - first_ts).count();
      read(ts);
    }
    void read(int64_t timestamp)
    {
      auto it = m_vec.lower_bound(timestamp);
      if(it != m_vec.end())
      {
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
    }
  };
  std::shared_ptr<recorder_thread> record_impl = std::make_shared<recorder_thread>();
  std::shared_ptr<player_thread> play_impl = std::make_shared<player_thread>();

  // Object definition
  struct inputs_t
  {
    PatternSelector pattern;
    halp::time_chooser<"Interval"> time;
    struct : halp::lineedit<"File pattern", "">
    {
      void update(DeviceRecorder& self) { self.update(); }
    } filename;
    struct
    {
      halp__enum("Mode", None, None, Record, Playback)
      void update(DeviceRecorder& self) { self.setMode(); }
    } mode;
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

    void operator()()
    {
      using namespace std;
      swap(recorder->filename, path);
      swap(recorder->roots, roots);
      player->filename = recorder->filename;
      player->roots = recorder->roots;
      recorder->reopen();
      player->reopen();
    }
  };

  struct reset_path_message
  {
    std::shared_ptr<recorder_thread> recorder;
    std::shared_ptr<player_thread> player;
    std::string path;
    void operator()()
    {
      using namespace std;
      swap(recorder->filename, path);
      player->filename = recorder->filename;
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
      player->setActive(mode == mode_type::Playback);
    }
  };

  using worker_message = ossia::variant<
      reset_message, reset_path_message, process_message, playback_message,
      activate_message>;

  struct
  {
    std::function<void(worker_message)> request;
    static void work(worker_message&& mess)
    {
      ossia::visit(
          [&]<typename M>(M&& msg) { std::forward<M>(msg)(); }, std::move(mess));
    }
  } worker;

  using tick = halp::tick_musical;

  void setMode()
  {
    worker.request(activate_message{record_impl, play_impl, inputs.mode.value});
  }
  void update()
  {
    worker.request(reset_path_message{record_impl, play_impl, inputs.filename});
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
      worker.request(reset_message{record_impl, play_impl, inputs.filename, roots});
    }

    switch(inputs.mode)
    {
      case decltype(inputs.mode)::None:
        break;
      case decltype(inputs.mode)::Record:
        worker.request(process_message{record_impl});
        break;
      case decltype(inputs.mode)::Playback:
        worker.request(playback_message{play_impl});
        break;
    }
  }

  bool started{};
  std::optional<int64_t> first_message_sent_pos;
  std::optional<int64_t> last_message_sent_pos;
};
}
