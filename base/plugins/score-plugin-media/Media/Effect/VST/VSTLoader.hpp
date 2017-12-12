#pragma once
//  Taken and refactored from ofxVstHostPluginLoader.h
//  https://github.com/Meach/ofxVstHost

#include <string>
#include "aeffectx.h"

namespace Media
{
namespace VST
{
using PluginEntryProc = AEffect* (*) (audioMasterCallback audioMaster);

struct VSTModule
{
    std::string path;
    void* module{};

    VSTModule(std::string fileName);
    ~VSTModule();
    PluginEntryProc getMain();
};

}
}

