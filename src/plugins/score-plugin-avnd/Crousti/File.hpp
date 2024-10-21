#pragma once
#include <Media/AudioDecoder.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/network/value/value.hpp>

#include <QByteArray>
#include <QFile>

#include <avnd/binding/ossia/soundfiles.hpp>
#include <libremidi/reader.hpp>

namespace oscr
{

namespace
{
[[nodiscard]] static QString
filenameFromPort(const ossia::value& value, const score::DocumentContext& ctx)
{
  if(auto str = value.target<std::string>())
    return score::locateFilePath(QString::fromStdString(*str).trimmed(), ctx);
  return {};
}

// TODO refactor this into a generic explicit soundfile loaded mechanism
[[nodiscard]] static auto
loadSoundfile(const ossia::value& value, const score::DocumentContext& ctx, double rate)
{
  // Initialize the control with the current soundfile
  if(auto str = filenameFromPort(value, ctx); !str.isEmpty())
  {
    auto dec = Media::AudioDecoder::decode_synchronous(str, rate);

    if(dec.has_value())
    {
      auto hdl = std::make_shared<ossia::audio_data>();
      hdl->data = std::move(dec->second);
      hdl->path = str.toStdString();
      hdl->rate = rate;
      return hdl;
    }
  }
  return ossia::audio_handle{};
}

using midifile_handle = std::shared_ptr<oscr::midifile_data>;
[[nodiscard]] inline midifile_handle
loadMidifile(const ossia::value& value, const score::DocumentContext& ctx)
{
  // Initialize the control with the current soundfile
  if(auto str = filenameFromPort(value, ctx); !str.isEmpty())
  {
    QFile f(str);
    if(!f.open(QIODevice::ReadOnly))
      return {};
    auto ptr = f.map(0, f.size());

    auto hdl = std::make_shared<oscr::midifile_data>();
    if(auto ret = hdl->reader.parse((uint8_t*)ptr, f.size());
       ret == libremidi::reader::invalid)
      return {};

    hdl->filename = str.toStdString();
    return hdl;
  }
  return {};
}

using raw_file_handle = std::shared_ptr<raw_file_data>;
[[nodiscard]] inline raw_file_handle loadRawfile(
    const ossia::value& value, const score::DocumentContext& ctx, bool text, bool mmap)
{
  // Initialize the control with the current soundfile
  if(auto filename = filenameFromPort(value, ctx); !filename.isEmpty())
  {
    if(!QFile::exists(filename))
      return {};

    auto hdl = std::make_shared<oscr::raw_file_data>();
    hdl->file.setFileName(filename);
    if(!hdl->file.open(QIODevice::ReadOnly))
      return {};

    if(mmap)
    {
      auto map = (char*)hdl->file.map(0, hdl->file.size());
      hdl->data = QByteArray::fromRawData(map, hdl->file.size());
    }
    else
    {
      if(text)
        hdl->file.setTextModeEnabled(true);

      hdl->data = hdl->file.readAll();
    }
    hdl->filename = filename.toStdString();
    return hdl;
  }
  return {};
}

[[nodiscard]] inline auto loadSoundfile(
    const ossia::value& value, const score::DocumentContext& ctx,
    const std::shared_ptr<ossia::execution_state>& st)
{
  const double rate = ossia::exec_state_facade{st.get()}.sampleRate();
  return loadSoundfile(value, ctx, rate);
}

template <typename Field>
static auto executePortPreprocess(auto& file)
{
  using field_file_type = decltype(Field::file);
  field_file_type ffile;
  ffile.bytes = decltype(ffile.bytes)(file.data.constData(), file.file.size());
  ffile.filename = file.filename;
  return Field::process(ffile);
}

}

}
