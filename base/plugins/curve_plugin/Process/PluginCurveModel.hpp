/*
Copyright: LaBRI / SCRIME

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/

#ifndef PLUGINCURVEMODEL_HPP
#define PLUGINCURVEMODEL_HPP
#include <QObject>
#include <QGraphicsObject>
#include <QRectF>

class PluginCurvePoint;
class PluginCurveSection;

class PluginCurveModel : public QObject
{
        Q_OBJECT
    private:
        bool _state; // Active or Inactive
        // QGraphicsObject* _pParent; // Deck for get bounding rect, width and height
        QList<PluginCurvePoint*> _points;  // Sort list of user's points
        QList<PluginCurveSection*> _sections;  // List of sections (not sort)
        const QRectF _limitRect;//=QRectF(0 + PluginCurvePoint::SHAPERADIUS,
        //       0 + PluginCurvePoint::SHAPERADIUS,
        //       _pParent->boundingRect().width() - 2*PluginCurvePoint::SHAPERADIUS - 2,
        //       _pParent->boundingRect().height() - 2*PluginCurvePoint::SHAPERADIUS); // Define the points area

    public:
        // Constructor
        PluginCurveModel(QObject* parentObject);
        // Returns the list of points
        QList<PluginCurvePoint*> points();
        // Returns the points area
        QRectF limitRect();
        // Gives the position of a point in the list
        int pointIndexOf(PluginCurvePoint* point);
        // Returns the point at the index position index in the list
        PluginCurvePoint* pointAt(int index);
        // Returns the size of the list
        int pointSize();
        // Returns the index of the point which would precede a point at position point.
        int pointSearchIndex(QPointF point);
        // Returns point's previous point
        PluginCurvePoint* previousPoint(PluginCurvePoint* point);
        // Return point's next point
        PluginCurvePoint* nextPoint(PluginCurvePoint* point);

    public slots:
        // Changes the state (Activ/Inactiv) of the curve
        void setState(bool b);
        // Inserts a point at the index index
        void pointInsert(int index, PluginCurvePoint* point);
        // Remove a point from the list
        void pointRemoveOne(PluginCurvePoint* point);
        // Swap the points at index index1 and index2
        void pointSwap(int index1, int index2);
        // Append a section
        void sectionAppend(PluginCurveSection* section);
        // Remove a section from the list
        void sectionRemoveOne(PluginCurveSection* section);
};

#endif // PLUGINCURVEMODEL_HPP*/
