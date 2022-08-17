/*
 * Copyright (C) 2012 Andreas Degert, Hermann Meyer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <cmath>
namespace
{

struct table1dc { // 1-dimensional function table
    float low;
    float high;
    float istep;
    int size;
    float data[];
};

template <int tab_size>
struct table1dc_imp {
    float low;
    float high;
    float istep;
    int size;
    float data[tab_size];
    operator table1dc&() const { return *(table1dc*)this; }
};

#include "clipt.cc"
#include "clipt1.cc"
#include "clipt2.cc"
#include "clipt3.cc"
#include "clipt4.cc"

table1dc *cliptable[10] = {
    &static_cast<table1dc&>(clippingtable[0]),
    &static_cast<table1dc&>(clippingtable[1]),
    &static_cast<table1dc&>(clippingtable2[0]),
    &static_cast<table1dc&>(clippingtable2[1]),
    &static_cast<table1dc&>(clippingtable3[0]),
    &static_cast<table1dc&>(clippingtable3[1]),
    &static_cast<table1dc&>(clippingtable1[0]),
    &static_cast<table1dc&>(clippingtable1[1]),
    &static_cast<table1dc&>(clippingtable4[0]),
    &static_cast<table1dc&>(clippingtable4[1]),
};

GUITARIX_EXPORT double asymclip(double x) {
	int table = 0;
    if (x<0) table = 1;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

GUITARIX_EXPORT double asymclip2(double x) {
	int table = 2;
    if (x<0) table = 3;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

GUITARIX_EXPORT double asymclip3(double x) {
	int table = 6;
    if (x<0) table = 7;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

GUITARIX_EXPORT double asymclip4(double x) {
	int table = 6;
    if (x<0) table = 1;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

GUITARIX_EXPORT double opamp(double x) {
	int table = 4;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

GUITARIX_EXPORT double opamp1(double x) {
	int table = 9;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

GUITARIX_EXPORT double opamp2(double x) {
	int table = 8;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

GUITARIX_EXPORT double asymhardclip(double x) {
	int table = 0;
    if (x<0) table = 1;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f ) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}


GUITARIX_EXPORT double asymhardclip2(double x) {
	int table = 2;
    if (x<0) table = 3;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f  - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, x);
}

GUITARIX_EXPORT double symclip(double x) {
	int table = 6;
    const table1dc& clip = *cliptable[table];
    double f = fabs(x);
    f = (f/(3.0 + f) - clip.low) * clip.istep;
    int i = static_cast<int>(f);
    if (i < 0) {
        f = clip.data[0];
    } else if (i >= clip.size-1) {
        f = clip.data[clip.size-1];
    } else {
	f -= i;
	f = clip.data[i]*(1-f) + clip.data[i+1]*f;
    }
    return copysign(f, -x);
}

}
