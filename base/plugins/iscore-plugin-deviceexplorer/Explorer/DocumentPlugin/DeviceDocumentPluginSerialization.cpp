#include "DeviceDocumentPlugin.hpp"

template<>
void Visitor<Reader<DataStream>>::readFrom_impl(
        const Explorer::DeviceDocumentPlugin& dev)
{
    readFrom(dev.rootNode());
}


template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(
        const Explorer::DeviceDocumentPlugin& dev)
{
    readFrom(dev.rootNode());
}

template<>
void Visitor<Writer<DataStream>>::writeTo(
        Explorer::DeviceDocumentPlugin& dev)
{
    writeTo(dev.rootNode());
}


template<>
void Visitor<Writer<JSONObject>>::writeTo(
        Explorer::DeviceDocumentPlugin& dev)
{
    writeTo(dev.rootNode());
}
