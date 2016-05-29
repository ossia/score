#pragma once
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <QQmlApplicationEngine>

#include <QAbstractListModel>
#include <QtWebSockets/QtWebSockets>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
namespace iscore {
class Settings;
class View;
class Presenter;
}  // namespace iscore

class QApplication;

class TriggerList : public QAbstractListModel
{
    public:

        void reset()
        {
            beginResetModel();
            endResetModel();
        }

        int rowCount(const QModelIndex& parent) const override
        {
            return timeNodes.size();
        }
        QVariant data(const QModelIndex& index, int role) const override
        {
            if(index.row() >= timeNodes.size())
                return {};

            auto & path = timeNodes[index.row()];
            return path.unsafePath().toString();
        }

        std::vector<Path<Scenario::TimeNodeModel>> timeNodes;
};

struct WebSocketHandler : public QObject
{

        Q_OBJECT


    public:
        TriggerList m_activeTimeNodes;
    private:
        QWebSocket m_server;
        std::map<QString, std::function<void(const QJsonObject&)>> m_answers;


    public:
        WebSocketHandler()
        {
            m_answers.insert(std::make_pair("TriggerAdded", [this] (const QJsonObject& obj)
            {
                auto json_it = obj.find("Path");
                if(json_it == obj.end())
                    return;

                auto path = unmarshall<Path<Scenario::TimeNodeModel>>(json_it->toObject());
                if(!path.valid())
                    return;

                auto it = find(m_activeTimeNodes.timeNodes, path);
                if(it == m_activeTimeNodes.timeNodes.end())
                {
                    m_activeTimeNodes.timeNodes.push_back(path);
                    m_activeTimeNodes.reset();
                }

            }));

            m_answers.insert(std::make_pair("TriggerRemoved", [this] (const QJsonObject& obj)
            {
                auto json_it = obj.find("Path");
                if(json_it == obj.end())
                    return;

                auto path = unmarshall<Path<Scenario::TimeNodeModel>>(json_it->toObject());
                if(!path.valid())
                    return;

                auto it = find(m_activeTimeNodes.timeNodes, path);
                if(it != m_activeTimeNodes.timeNodes.end())
                {
                    m_activeTimeNodes.timeNodes.erase(it);
                    m_activeTimeNodes.reset();
                }
            }));


            connect(&m_server, &QWebSocket::textMessageReceived,
                    this, &WebSocketHandler::processTextMessage);
            connect(&m_server, &QWebSocket::binaryMessageReceived,
                    this, &WebSocketHandler::processBinaryMessage);
            connect(&m_server, &QWebSocket::connected,
                    this, [] { qDebug("yolooo"); });

            m_server.open(QUrl("ws://localhost:10212"));
        }


        ~WebSocketHandler()
        {
            m_server.close();
        }




        void processTextMessage(const QString& message)
        {
            processBinaryMessage(message.toLatin1());
        }


        void processBinaryMessage(QByteArray message)
        {
            QJsonParseError error;
            auto doc = QJsonDocument::fromJson(std::move(message), &error);
            if(error.error)
                return;

            auto obj = doc.object();
            auto it = obj.find("Message");
            if(it == obj.end())
                return;

            auto mess = it->toString();
            auto answer_it = m_answers.find(mess);
            if(answer_it == m_answers.end())
                return;

            answer_it->second(obj);
        }


    public slots:

        void rowPressed(int i)
        {
            if(i >= m_activeTimeNodes.timeNodes.size())
                return;

            auto tn = m_activeTimeNodes.timeNodes[i];

            QJsonObject mess;
            mess["Message"] = "Trigger";
            mess["Path"] = toJsonObject(tn);
            QJsonDocument doc{mess};
            auto json = doc.toJson();

            m_server.sendTextMessage(json);
        }
};


class RemoteApplication final :
        public NamedObject,
        public iscore::ApplicationInterface
{
    public:
        RemoteApplication(int& argc, char** argv);
        ~RemoteApplication();

        const iscore::ApplicationContext& context() const override;

        int exec();

        // Base stuff.
        QApplication* m_app;

        std::unique_ptr<iscore::Settings> m_settings; // Global settings

        // MVP
        iscore::View* m_view {};
        iscore::Presenter* m_presenter {};

        iscore::ApplicationSettings m_applicationSettings;

        QQmlApplicationEngine engine;
        WebSocketHandler m_triggers;
};
