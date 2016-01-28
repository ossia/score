#pragma once
#include <iscore_lib_base_export.h>
class QGraphicsObject;
class QGraphicsItem;
/**
 * @brief deleteGraphicsItem Properly delete a QGraphicsObject
 * @param item item to delete
 *
 * Simply using deleteLater() is generally not enough, the
 * item has to be removed from the scene else there will be crashes.
 */
ISCORE_LIB_BASE_EXPORT void deleteGraphicsObject(QGraphicsObject* item);
ISCORE_LIB_BASE_EXPORT void deleteGraphicsItem(QGraphicsItem* item);
