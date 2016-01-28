#pragma once
#include <string>
#include <QDataStream>
#include <QDebug>

inline QDataStream& operator<< (QDataStream& stream, const std::string& obj)
{
    uint32_t size = obj.size();
    stream << size;

    stream.writeRawData(obj.data(), size);
    return stream;
}

inline QDataStream& operator>> (QDataStream& stream, std::string& obj)
{
    uint32_t n = 0;
    stream >> n;
    obj.resize(n);

    char* addr = n > 0 ? &obj[0] : nullptr;
    stream.readRawData(addr, n);

    return stream;
}

inline QDebug operator<< (QDebug debug, const std::string& obj)
{
    debug << obj.c_str();
    return debug;
}
