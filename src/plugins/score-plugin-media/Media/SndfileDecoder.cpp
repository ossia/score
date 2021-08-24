#include <Media/SndfileDecoder.hpp>
#include <Media/MediaFileHandle.hpp>

#include <sndfile.h>
#include <new>
#include <QDebug>

namespace Media
{

SndfileDecoder::SndfileDecoder()
{

}

struct sndfile_deleter {
  void operator()(SNDFILE* ptr) {
    if(ptr)
      sf_close(ptr);
  }
};

using sndfile_ptr = std::unique_ptr<SNDFILE, sndfile_deleter>;
void SndfileDecoder::decode(const QString& path, audio_handle hdl)
{
  auto p = path.toStdString();

  SF_INFO info;
  auto sf = sndfile_ptr{sf_open(p.c_str(), SFM_READ, &info)};
  if(!sf)
  {
    qDebug() << "Could not open sound file: " << sf_strerror(nullptr);
    return;
  }

  // Read the data
  switch(info.channels)
  {
    case 0:
    {
      return;
    }
    case 1:
    {
      hdl->data.resize(1);
      hdl->data[0].resize(info.frames);
      sf_readf_float(sf.get(), hdl->data[0].data(), info.frames);
      break;
    }
    default:
    {
      auto buf = new (std::nothrow) float[info.channels * info.frames];
      if(!buf)
      {
        qDebug() << "Could not allocate memory for soundfile";
        return;
      }
      std::unique_ptr<float[]> memory_holder{buf}; // just for deletion

      sf_readf_float(sf.get(), buf, info.frames);

      // Allocate memory
      hdl->data.resize(info.channels);
      for(auto& channel : hdl->data)
        channel.resize(info.frames);

      // Copy by deinterleaving
      auto& res = hdl->data;
      std::size_t frame = 0;
      for(std::size_t i = 0, N = info.frames * info.channels; i < N; i += info.channels) {

        for(int s = 0; s < info.channels; ++s)
          res[s][frame] = buf[i + s];

        frame++;
      }
      break;
    }
  }

  fileSampleRate = info.samplerate;
  convertedSampleRate = fileSampleRate;
  channels = info.channels;
  decoded = info.frames;
}

std::optional<AudioInfo> SndfileDecoder::do_probe(const QString& path)
{
  auto p = path.toStdString();

  SF_INFO info;
  auto sf = sndfile_ptr{sf_open(p.c_str(), SFM_READ, &info)};
  if(!sf)
  {
    qDebug() << "Could not open sound file: " << sf_strerror(nullptr);
    return std::nullopt;
  }

  AudioInfo ret;
  ret.channels = info.channels;
  ret.fileLength = info.frames;
  ret.max_arr_length = info.frames;
  ret.fileRate = info.samplerate;

  sf_seek(sf.get(), info.frames, SF_SEEK_END);
  SF_LOOP_INFO loop;
  sf_command(sf.get(), SFC_GET_LOOP_INFO, &loop, sizeof (loop)) ;
  if(loop.bpm > 0.001)
  {
    ret.tempo = loop.bpm;
  }
  else
  {
    ret.tempo = estimateTempo(path);
  }

  return ret;
}

}
