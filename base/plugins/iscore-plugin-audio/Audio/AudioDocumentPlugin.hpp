#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <Audio/AudioEngine.hpp>
class AudioDocumentPlugin : public iscore::DocumentPluginModel
{
    public:
        AudioDocumentPlugin(
                iscore::Document& doc,
                QObject* parent):
            iscore::DocumentPluginModel{doc, "AudioDocumentPlugin", parent}
        {

        }

        AudioEngine& engine() { return m_engine; }
        void serialize(const VisitorVariant&) const override;

    private:
        AudioEngine m_engine;
};
