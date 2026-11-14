#pragma once
#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>

#include <ossia/audio/drwav_write_handle.hpp>
#include <ossia/detail/buffer_pool.hpp>
#include <ossia/network/value/detail/value_conversion_impl.hpp>

#include <QDateTime>

#include <AvndProcesses/AddressTools.hpp>
#include <AvndProcesses/Utils.hpp>
#include <halp/audio.hpp>
#include <halp/callback.hpp>

namespace avnd_tools
{
/** Records audio into a WAV file.
 *  
 *  Writing to the disk is done in a worker thread as is tradition.
 */
struct AudioRecorder
{
  halp_meta(name, "Audio recorder")
  halp_meta(author, "ossia team")
  halp_meta(category, "Audio/Recording")
  halp_meta(description, "Records audio to a file")
  halp_meta(c_name, "avnd_audio_recorder")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/audio-utilities.html#audio-recorder")
  halp_meta(uuid, "4463dddc-6acf-4106-a680-fed67d8030da")

  using audio_buffer = boost::container::vector<double>;

  // Threaded worker
  struct recorder_thread
  {
    explicit recorder_thread(const score::DocumentContext& context)
        : context{context}
    {
    }
    const score::DocumentContext& context;
    ossia::drwav_write_handle f;
    std::string filename;
    std::string actual_filename;
    int channels = 0;
    int rate = 0;
    bool must_record = false;

    //! The buffers the audio thread copies its ticks in: taken there, given
    //! back here once written.
    ossia::object_pool<audio_buffer> pool;
    std::size_t buffer_capacity = 0;

    //! Main thread, before the first tick.
    void reserve(std::size_t capacity, int count)
    {
      buffer_capacity = capacity;
      for(int i = 0; i < count; i++)
      {
        audio_buffer b;
        b.reserve(capacity);
        pool.release(std::move(b));
      }
    }

    //! Grows a buffer to what the ticks have needed, then puts it back.
    void give_back(audio_buffer b, std::size_t needed)
    {
      buffer_capacity = std::max(buffer_capacity, needed);
      if(b.capacity() < buffer_capacity)
        b.reserve(buffer_capacity);
      pool.release(std::move(b));
    }

    bool reopen()
    {
      if(must_record)
      {
        // Open the file with the correct substitutions
        actual_filename = filter_filename(this->filename, context).toStdString();
        if(actual_filename.empty())
          return false;

        f.open(actual_filename, channels, rate, 16);
        return false;
      }
      else
      {
        bool must_reply = false;
        if(f.is_open() && f.written_frames() > 0)
        {
          must_reply = true;
        }
        f.close();
        return must_reply;
      }
    }

    void write(int frames, audio_buffer& data)
    {
      int these_channels = data.size() / frames;
      if(these_channels != channels)
      {
        channels = these_channels;
        reopen();
      }

      if(!f.is_open())
        return;

      // Put addresses to the first sample of each channel in an array,
      // e.g. arr[0] points to the first channel, arr[1] to the second, etc.
      const double** arr = (const double**)alloca(channels * sizeof(double*));
      for(int c = 0; c < channels; ++c)
        arr[c] = data.data() + c * frames;

      // Write the data
      f.write_pcm_frames(frames, arr);
    }
  };
  std::shared_ptr<recorder_thread> impl;

  struct reset_message
  {
    std::string path;
    int rate{};
    bool must_record{};
    bool operator()(recorder_thread& self)
    {
      using namespace std;
      swap(self.filename, path);
      self.rate = rate;
      self.must_record = must_record;
      return self.reopen();
    }
  };

  struct process_message
  {
    int frames{};
    audio_buffer data;
    //! Non-zero when the buffer was too small for the tick, which was not
    //! recorded: the size it needed.
    std::size_t needed{};
    bool operator()(recorder_thread& self)
    {
      if(needed == 0)
        self.write(frames, data);
      self.give_back(std::move(data), needed);
      return false;
    }
  };
  using worker_message = ossia::variant<reset_message, process_message>;

  struct
  {
    std::function<void(std::shared_ptr<recorder_thread>, worker_message)> request;
    static std::function<void(AudioRecorder&)>
    work(std::shared_ptr<recorder_thread> t, worker_message&& mess)
    {
      bool reply = ossia::visit(
          [&]<typename M>(M&& msg) { return std::forward<M>(msg)(*t); },
          std::move(mess));
      if(reply)
      {
        return [filename = t->actual_filename](AudioRecorder& self) mutable {
          using namespace std;
          swap(self.filename_to_output, filename);
        };
      }
      else
      {
        return {};
      }
    }
  } worker;

  // Object definition
  struct
  {
    halp::dynamic_audio_bus<"Audio", double> audio;
    struct : halp::lineedit<"File pattern", "">
    {
      void update(AudioRecorder& self) { self.update(); }
    } filename;

    struct : halp::toggle<"Record">
    {
      void update(AudioRecorder& self)
      {
        if(prev != value)
        {
          prev = value;
          self.update();
        }
      }
      bool prev{false};
    } record;
  } inputs;

  struct
  {
    halp::callback<"Filename", std::string> finished;
  } outputs;

  const score::DocumentContext* ossia_document_context{};
  int current_rate = 0;
  //! Ticks not recorded because no buffer was free or large enough.
  int dropped_blocks = 0;

  //! The binding prepares again, from the audio thread, when the input's
  //! channel count or the engine buffer change: the worker follows the
  //! channel count of what it is given, and only a new rate is told to it.
  void prepare(halp::setup s)
  {
    SCORE_ASSERT(ossia_document_context);
    if(!impl)
    {
      impl = std::make_shared<recorder_thread>(*ossia_document_context);
      // A quarter of a second of ticks, of up to 16 channels or the input's.
      const int frames = std::max(1, s.frames);
      const int rate = std::max(1, int(s.rate));
      impl->reserve(
          std::size_t(std::max(16, s.input_channels)) * std::size_t(frames),
          std::max(4, rate / 4 / frames + 1));
    }
    if(std::exchange(current_rate, int(s.rate)) != int(s.rate))
      update();
  }

  void update()
  {
    if(impl)
      worker.request(impl, reset_message{inputs.filename, current_rate, inputs.record});
  }

  void operator()(int frames)
  {
    if(!std::exchange(started, true))
      update();

    const auto needed = std::size_t(inputs.audio.channels) * std::size_t(frames);
    audio_buffer data = impl->pool.acquire();
    if(data.capacity() == 0)
    {
      // The worker has not given the buffers back yet.
      dropped_blocks++;
    }
    else if(data.capacity() < needed)
    {
      dropped_blocks++;
      worker.request(impl, process_message{frames, std::move(data), needed});
    }
    else
    {
      data.resize(needed, boost::container::default_init);
      for(int c = 0; c < inputs.audio.channels; c++)
      {
        double* src = inputs.audio.samples[c];
        double* ptr = data.data() + c * frames;
        for(int i = 0; i < frames; i++)
          ptr[i] = src[i];
      }

      worker.request(impl, process_message{frames, std::move(data)});
    }

    if(!filename_to_output.empty())
    {
      outputs.finished(filename_to_output);
      filename_to_output.clear();
    }
  }

  bool started{};
  std::optional<int64_t> first_message_sent_pos;
  std::optional<int64_t> last_message_sent_pos;

  std::string filename_to_output;
};

}
