#pragma once
#include <Device/Node/DeviceNode.hpp>

#include <score_plugin_deviceexplorer_export.h>

#include <QString>

namespace Explorer
{
class DeviceDocumentPlugin;

/**
 * @brief Undo what a learn added, leaving the device itself alone.
 *
 * Cancelling a learn used to roll back with ReloadWholeDevice::undo, which
 * removes the device and builds a new one from the saved tree. Everything the
 * outside world had attached to the old one goes with it: a PipeWire MIDI link,
 * an open socket, a connected client. Cancelling a learn that added nothing at
 * all still cost the user their cable.
 *
 * The addresses a learn added are few and known, so they are taken out one by
 * one instead -- from the device and from the explorer both, which is what
 * NodeUpdateProxy::removeNode does.
 *
 * @param before The device's node as it was before learning started.
 */
SCORE_PLUGIN_DEVICEEXPLORER_EXPORT
void rollbackLearnedNodes(
    DeviceDocumentPlugin& plug, const QString& deviceName, const Device::Node& before);
}
