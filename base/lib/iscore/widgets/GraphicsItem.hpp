#pragma once
class QGraphicsObject;
/**
 * @brief deleteGraphicsItem Properly delete a QGraphicsObject
 * @param item item to delete
 *
 * Simply using deleteLater() is generally not enough, the
 * item has to be removed from the scene else there will be crashes.
 */
void deleteGraphicsObject(QGraphicsObject* item);
