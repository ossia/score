#include "DeviceExplorerPanelModel.hpp"

#include "Panel/DeviceExplorerModel.hpp"
#include <core/document/DocumentModel.hpp>

DeviceExplorerPanelModel::DeviceExplorerPanelModel(iscore::DocumentModel* parent) :
    iscore::PanelModelInterface {"DeviceExplorerPanelModel", parent},
    m_model {new DeviceExplorerModel{this}}
{
}

DeviceExplorerPanelModel::DeviceExplorerPanelModel(const VisitorVariant& data, iscore::DocumentModel *parent):
    iscore::PanelModelInterface {"DeviceExplorerPanelModel", parent},
    m_model {new DeviceExplorerModel{data, this}}
{

}

void DeviceExplorerPanelModel::serialize(const VisitorVariant &vis) const
{
    if(vis.identifier == DataStream::type())
    {
        auto& ser = static_cast<DataStream::Serializer&>(vis.visitor);
        ser.m_stream << (bool) m_model->rootNode();
        if(m_model->rootNode())
            ser.readFrom(*m_model->rootNode());
    }
    else if(vis.identifier == JSONObject::type())
    {
        auto& ser = static_cast<JSONObject::Serializer&>(vis.visitor);
        if(m_model->rootNode())
            ser.readFrom(*m_model->rootNode());
    }
}

DeviceExplorerModel*DeviceExplorerPanelModel::deviceExplorer()
{
    return m_model;
}

