#pragma once
#include <ossia/audio/drwav_write_handle.hpp>
#include <ossia/network/value/detail/value_conversion_impl.hpp>

#include <AvndProcesses/AddressTools.hpp>
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
  halp_meta(uuid, "4463dddc-6acf-4106-a680-fed67d8030da")

  using audio_buffer = boost::container::vector<double>;
  // Threaded worker
  struct recorder_thread
  {
    ossia::drwav_write_handle f;
    std::string filename;
    int channels = 0;
    int rate = 0;

    void reopen()
    {
      f.close();
      if(channels <= 0)
        return;
      if(rate <= 0)
        return;

      if(filename.empty())
        return;

      auto fname = QByteArray::fromStdString(this->filename);
      fname.replace("%t", QDateTime::currentDateTimeUtc().toString().toUtf8());
      f.open(fname.toStdString(), channels, rate, 16);
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

      const double** arr = (const double**)alloca(channels * sizeof(double*));
      for(int c = 0; c < channels; ++c)
        arr[c] = data.data() + c * frames;
      f.write_pcm_frames(frames, arr);
    }
  };
  std::shared_ptr<recorder_thread> impl = std::make_shared<recorder_thread>();

  struct reset_message
  {
    std::string path;
    int rate{};
    void operator()(recorder_thread& self)
    {
      using namespace std;
      swap(self.filename, path);
      self.rate = rate;
      self.reopen();
    }
  };

  struct process_message
  {
    int frames{};
    audio_buffer data;
    void operator()(recorder_thread& self) { self.write(frames, data); }
  };
  using worker_message = ossia::variant<reset_message, process_message>;

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
    halp::dynamic_audio_bus<"Audio", double> audio;
    struct : halp::lineedit<"File pattern", "">
    {
      void update(AudioRecorder& self) { self.update(); }
    } filename;

    struct : halp::impulse_button_t<"Update">
    {
      void update(AudioRecorder& self) { self.update(); }
    } update;
  } inputs;

  struct
  {
  } outputs;

  int current_rate = 0;
  void prepare(halp::setup s) { current_rate = s.rate; }

  void update() { worker.request(impl, reset_message{inputs.filename, current_rate}); }

  void operator()(int frames)
  {
    if(!std::exchange(started, true))
    {
      update();
    }

    data.resize(inputs.audio.channels * frames, boost::container::default_init);
    for(int c = 0; c < inputs.audio.channels; c++)
    {
      double* src = inputs.audio.samples[c];
      double* ptr = data.data() + c * frames;
      for(int i = 0; i < frames; i++)
        ptr[i] = src[i];
    }

    worker.request(impl, process_message{frames, std::move(data)});
  }

  bool started{};
  std::optional<int64_t> first_message_sent_pos;
  std::optional<int64_t> last_message_sent_pos;

  audio_buffer data;
};

}
