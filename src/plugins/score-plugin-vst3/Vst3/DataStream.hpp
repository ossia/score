#pragma once
#include <pluginterfaces/base/ibstream.h>
#include <QDataStream>

namespace vst3
{
struct Vst3DataStream
    : public Steinberg::IBStream
{
public:
  QDataStream& stream;
  explicit Vst3DataStream(QDataStream& s): stream{s}
  {
    s.setByteOrder(QDataStream::LittleEndian);
  }
  Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void **obj) override
  {
    return Steinberg::kResultFalse;
  }
  Steinberg::uint32 addRef() override
  {
    return 1;
  }
  Steinberg::uint32 release() override
  {
    return 1;
  }

  Steinberg::tresult read(void *buffer, Steinberg::int32 numBytes, Steinberg::int32 *numBytesRead) override
  {
    int count = stream.readRawData((char*)buffer, numBytes);
    qDebug() << "Read: " << count;
    if(numBytesRead)
      *numBytesRead = count;
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult write(void *buffer, Steinberg::int32 numBytes, Steinberg::int32 *numBytesWritten) override
  {
    int count = stream.writeRawData((char*) buffer, numBytes);
    if(numBytesWritten)
      *numBytesWritten = count;

    return Steinberg::kResultTrue;
  }
  Steinberg::tresult seek(Steinberg::int64 pos, Steinberg::int32 mode, Steinberg::int64 *result) override
  {
    bool ok = stream.device()->seek(pos);
    if(result)
      *result = stream.device()->pos();

    return ok ? Steinberg::kResultTrue : Steinberg::kResultFalse;
  }
  Steinberg::tresult tell(Steinberg::int64 *pos) override
  {
    if(pos)
      *pos = stream.device()->pos();

    return Steinberg::kResultTrue;
  }
};
}
