#pragma once
#include <Media/AudioDecoder.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/File.hpp>

#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/network/value/value.hpp>

#include <QByteArray>
#include <QDebug>
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

// Resolve <PROJECT>: / <LIBRARY>: / document-relative paths for a path-valued
// string control (e.g. a folder_port) exactly like the file ports above do, so
// the object sees an absolute path instead of the raw library-relative string.
// A non-string or empty value is returned unchanged (an unset control stays
// empty rather than resolving to the document folder).
[[nodiscard]] static ossia::value
resolvePathValue(const ossia::value& value, const score::DocumentContext& ctx)
{
  if(auto str = value.target<std::string>(); str && !str->empty())
  {
    const QString resolved
        = score::locateFilePath(QString::fromStdString(*str).trimmed(), ctx);
    if(!resolved.isEmpty())
      return ossia::value{resolved.toStdString()};
  }
  return value;
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
  // A file port whose control holds no path at all is simply unset: that is
  // not a failure and must stay silent. Everything below IS a failure, and
  // every caller of this function treats a null handle as "do nothing" --
  // ExecutorPortSetup.hpp:298/:327 and GpuUtils.hpp:202 all wrap the call in
  // `if(auto hdl = loadRawfile(...))`, so the object's own preprocessing
  // (Field::process) is never even reached. Without a word here, pointing a
  // file port at a path that does not exist produces NOTHING anywhere: no
  // load, no callback, no message. That is the normal case, not an edge one --
  // a document written on another machine carries absolute paths that do not
  // resolve here.
  const auto* raw = value.target<std::string>();
  if(!raw || raw->empty())
    return {};

  const QString filename = filenameFromPort(value, ctx);
  if(filename.isEmpty())
  {
    qWarning() << "file port: could not resolve"
               << QString::fromStdString(*raw);
    return {};
  }

  {
    if(!QFile::exists(filename))
    {
      qWarning() << "file port: no such file:" << filename;
      return {};
    }

    auto hdl = std::make_shared<oscr::raw_file_data>();
    hdl->file.setFileName(filename);
    if(!hdl->file.open(QIODevice::ReadOnly))
    {
      qWarning() << "file port: cannot open" << filename << ":"
                 << hdl->file.errorString();
      return {};
    }

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
