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

#include "PluginCurveModel.hpp"
#include "Implementation/PluginCurvePoint.hpp"
#include "Implementation/PluginCurveSection.hpp"


PluginCurveModel::PluginCurveModel (int id, QObject* parentObject) :
	ProcessSharedModelInterface{id, "PluginCurveModel", parentObject}//,
//	_pParent (parentItem)
{
	_state = true; // Active when created
}

QList<PluginCurvePoint*> PluginCurveModel::points()
{
	return _points;
}

QRectF PluginCurveModel::limitRect()
{
	return _limitRect;
}

int PluginCurveModel::pointIndexOf (PluginCurvePoint* point)
{
	return _points.indexOf (point);
}

PluginCurvePoint* PluginCurveModel::pointAt (int index)
{
	return _points.at (index);
}

int PluginCurveModel::pointSize()
{
	return _points.size();
}

template<typename T1, typename T2>
bool compare (const T1& value1, const T2& value2, bool (* (comparisonFunc) (const T1&, const T2&) ) )
{
	if (comparisonFunc == nullptr)
	{
		return (value1 >= value2);
	}
	else
	{
		return ( (*comparisonFunc) (value1, value2) );
	}
}

// Return where value must be inserted for keep the list sorted.
template<typename T1, typename T2>
int dichotomousSearch (QList<T1*> list, T2 value, int first, int last, bool (*supFunc (const T1&, const T2&) ) = nullptr, bool (*infFunc (const T1&, const T2&) ) = nullptr)
{
	if (list.size() == 0)
	{
		return -1;
	}

	if ( (supFunc == nullptr && *list.at (first) >= value ) ||
	        (supFunc != nullptr && supFunc (*list.at (first), value) ) )
//  if (*list.at(first) >= value )
	{
		return -1;
	}

	if (last - first <= 1)
	{
		if ( (infFunc == nullptr && *list.at (last) <= value ) ||
		        (infFunc != nullptr && infFunc (*list.at (last), value) ) )
//      if ( *(list.at(last)) <= value )
		{
			return last;
		}
		else
		{
			return first;
		}
	}

	int mid = (first + last) / 2;

	if ( (supFunc == nullptr && *list.at (mid) >= value ) ||
	        (supFunc != nullptr && supFunc (*list.at (mid), value) ) )
//  if (*list.at(mid)>=value)
	{
		return (dichotomousSearch (list, value, first, mid - 1, supFunc, infFunc) );
	}
	else
	{
		return (dichotomousSearch (list, value, mid, last, supFunc, infFunc) );
	}
}

int PluginCurveModel::pointSearchIndex (QPointF point)
{
	return dichotomousSearch (_points, point, 0, _points.size() - 1);
}

PluginCurvePoint* PluginCurveModel::previousPoint (PluginCurvePoint* point)
{
	if (point == nullptr)
	{
		return nullptr;
	}

	PluginCurveSection* leftSection = point->leftSection();

	if (leftSection != nullptr)
	{
		return leftSection->sourcePoint();
	}
	else
	{
		return nullptr;
	}
}

PluginCurvePoint* PluginCurveModel::nextPoint (PluginCurvePoint* point)
{
	if (point == nullptr)
	{
		return nullptr;
	}

	PluginCurveSection* rightSection = point->rightSection();

	if (rightSection != nullptr)
	{
		return rightSection->destPoint();
	}
	else
	{
		return nullptr;
	}
}

//bool PluginCurveModel::enoughSpaceBefore(PluginCurvePoint *point)
//{
//  PluginCurvePoint *previous = previousPoint(point);
//  if (previous != NULL)
//    return (point->x() - previous->x() >= 2*PluginCurvePresenter::POINTMINDIST);
//  else
//    return (point->x() - limitRect().x() >= PluginCurvePresenter::POINTMINDIST);
//}

//bool PluginCurveModel::enoughSpaceAfter(PluginCurvePoint *point)
//{
//  PluginCurvePoint *next = nextPoint(point);
//  if (next != NULL)
//    return (next->x() - point->x() >= 2*PluginCurvePresenter::POINTMINDIST);
//  else
//    return (limitRect().x() + limitRect().width() - point->x() >= PluginCurvePresenter::POINTMINDIST);
//}

void PluginCurveModel::setState (bool b)
{
	if (b != _state)
	{
		_state = b;
	}
}

void PluginCurveModel::pointRemoveOne (PluginCurvePoint* point)
{
	if (point != nullptr)
	{
		_points.removeOne (point);
	}
}

void PluginCurveModel::pointSwap (int index1, int index2)
{
	_points.swap (index1, index2);
}

void PluginCurveModel::pointInsert (int index, PluginCurvePoint* point)
{
	if (point != nullptr)
	{
		_points.insert (index, point);
	}
}

void PluginCurveModel::sectionAppend (PluginCurveSection* section)
{
	if (section == NULL)
	{
		return;
	}

	_sections.append (section);
}

void PluginCurveModel::sectionRemoveOne (PluginCurveSection* section)
{
	if (section == NULL)
	{
		return;
	}

	_sections.removeOne (section);
}



