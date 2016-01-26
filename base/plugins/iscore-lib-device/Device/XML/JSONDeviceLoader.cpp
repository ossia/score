/*#include "JSONDeviceLoader.hpp"
#include <QFile>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <QJsonDocument>

namespace rapidjson
{

template<typename T>
auto begin(T&& doc)
{
    return doc.MemberBegin();
}

template<typename T>
auto end(T&& doc)
{
    return doc.MemberEnd();
}

}
namespace Device
{
template<typename Member>
static void read_node(
        const Member& dom_element,
        Device::Node &parentNode)
{
    qDebug() << dom_element.name.GetString();
}

void loadDeviceFromJSON(
        const QString& filePath,
        Device::Node& node)
{
    // ouverture d'un xml
    QFile theFile{filePath};
    if(!theFile.open(QIODevice::ReadOnly))
    {
        qDebug() << "Erreur : Impossible d'ouvrir le ficher JSON";
        theFile.close();
        return;
    }

    QJsonDocument qt_doc = QJsonDocument::fromJson(theFile.readAll());
    if(qt_doc.isNull())
    {
        qDebug("fu");
        exit(0); return;
    }

    rapidjson::Document doc;
    auto data = theFile.readAll();
    if(data.contains("var preset = "))
    {
        doc.Parse(data.constData() + 13);
    }
    else
    {
        doc.Parse(data.constData());
    }

    if(doc.HasParseError())
    {
        rapidjson::ParseResult ok(doc.GetParseError(), doc.GetErrorOffset());
        qDebug() << "Erreur : Impossible de charger le ficher JSON";
        qDebug() << rapidjson::GetParseError_En(ok.Code()) << ok.Offset();
        theFile.close();
        exit(0);
        return;
    }

    for(auto& value : doc)
    {
        read_node(value, node);
    }
    /*


    // extraction des données

    QDomElement doc = domDoc->documentElement();
    QDomElement application = doc.firstChildElement("application");
    QDomElement dom_node = application.firstChildElement("");

    while(!dom_node.isNull())
    {
        convertFromDomElement(dom_node, node);
        dom_node = dom_node.nextSiblingElement("");
    }
    */ /*
}
}
*/
// static Device::Node node;
//  static int f = (loadDeviceFromJSON("/home/jcelerier/Téléchargements/exemple_preset.json", node), 1);
