#include "CodeWriter.hpp"

namespace Device
{
CodeWriter::CodeWriter(const DeviceInterface& p) noexcept
    : self{p}
{
}

CodeWriter::~CodeWriter() = default;
}
