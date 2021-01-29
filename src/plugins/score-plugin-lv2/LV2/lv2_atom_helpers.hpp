// lv2_atom_helpers.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

/*  Helper functions for LV2 atom:Sequence event buffer.
 *
 *  tentatively adapted from:
 *
 *  - lv2_evbuf.h,c - An abstract/opaque LV2 event buffer implementation.
 *
 *  - event-helpers.h - Helper functions for the LV2 Event extension.
 *    <http://lv2plug.in/ns/ext/event>
 *
 *    Copyright 2008-2012 David Robillard <http://drobilla.net>
 */

#ifndef LV2_ATOM_HELPERS_H
#define LV2_ATOM_HELPERS_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>

// An abstract/opaque LV2 atom:Sequence buffer.
//
struct LV2_Atom_Buffer
{
  static uint32_t sequence_type;
  static uint32_t chunk_type;

  uint32_t capacity;
  LV2_Atom_Sequence atoms;
  LV2_Atom_Buffer(uint32_t capacity, uint32_t ct, uint32_t seq_type, bool input)
      : capacity{capacity}
  {
    chunk_type = ct;
    sequence_type = seq_type;
    reset(input);
  }

  void reset(bool input)
  {
    if (input)
    {
      atoms.atom.size = sizeof(LV2_Atom_Sequence_Body);
      atoms.atom.type = sequence_type;
    }
    else
    {
      atoms.atom.size = capacity;
      atoms.atom.type = chunk_type;
    }
  }

  // Return the total padded size of events stored in a LV2 atom:Sequence
  // buffer.
  //
  uint32_t get_size()
  {
    if (atoms.atom.type == sequence_type)
      return atoms.atom.size - sizeof(LV2_Atom_Sequence_Body);
    else
      return 0;
  }

  // Return the actual LV2 atom:Sequence implementation.
  LV2_Atom_Sequence* get_sequence(LV2_Atom_Buffer* buf) { return &buf->atoms; }
};

// An iterator over an atom:Sequence buffer.
//
struct Iterator
{
  Iterator(LV2_Atom_Buffer* b) : buf{b} { }

  LV2_Atom_Buffer* buf{};
  uint32_t offset{};

  // Pad a size to 64 bits (for LV2 atom:Sequence event sizes).
  static uint32_t pad_size(uint32_t size) { return (size + 7) & (~7); }

  // Reset an iterator to point to the start of an LV2 atom:Sequence buffer.
  //
  bool begin(LV2_Atom_Buffer* buf)
  {
    this->buf = buf;
    offset = 0;

    return (buf->atoms.atom.size > 0);
  }

  // Reset an iterator to point to the end of an LV2 atom:Sequence buffer.
  //
  bool end(LV2_Atom_Buffer* buf)
  {
    this->buf = buf;
    offset = pad_size(buf->get_size());

    return (offset < buf->capacity - sizeof(LV2_Atom_Event));
  }

  // Check if a LV2 atom:Sequenece buffer iterator is valid.
  //
  bool is_valid() { return offset < buf->get_size(); }

  // Advance a LV2 atom:Sequenece buffer iterator forward one event.
  //
  bool increment()
  {
    if (!is_valid())
      return false;

    LV2_Atom_Sequence* atoms = &buf->atoms;
    uint32_t size
        = ((LV2_Atom_Event*)((char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, atoms) + offset))
              ->body.size;
    offset += pad_size(sizeof(LV2_Atom_Event) + size);

    return true;
  }

  // Get the event currently pointed at a LV2 atom:Sequence buffer iterator.
  //
  LV2_Atom_Event* get(uint8_t** data)
  {
    if (!is_valid())
      return NULL;

    auto atoms = &buf->atoms;
    auto ev = (LV2_Atom_Event*)((char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, atoms) + offset);

    *data = (uint8_t*)LV2_ATOM_BODY(&ev->body);

    return ev;
  }

  // Write an event at a LV2 atom:Sequence buffer iterator.
  bool
  write(uint32_t frames, uint32_t /*subframes*/, uint32_t type, uint32_t size, const uint8_t* data)
  {
    LV2_Atom_Sequence* atoms = &buf->atoms;
    if (buf->capacity - sizeof(LV2_Atom) - atoms->atom.size < sizeof(LV2_Atom_Event) + size)
      return false;

    LV2_Atom_Event* ev
        = (LV2_Atom_Event*)((char*)LV2_ATOM_CONTENTS(LV2_Atom_Sequence, atoms) + offset);

    ev->time.frames = frames;
    ev->body.type = type;
    ev->body.size = size;

    memcpy(LV2_ATOM_BODY(&ev->body), data, size);

    size = pad_size(sizeof(LV2_Atom_Event) + size);
    atoms->atom.size += size;
    offset += size;

    return true;
  }
};

struct AtomBuffer
{
  LV2_Atom_Buffer* buf{};
  AtomBuffer(uint32_t capacity, uint32_t chunk_type, uint32_t seq_type, bool input)
  {
    // Note : isn't the second sizeof redundant ?
    buf = (LV2_Atom_Buffer*)::operator new(
        sizeof(LV2_Atom_Buffer) + sizeof(LV2_Atom_Sequence) + capacity);
    new (buf) LV2_Atom_Buffer(capacity, chunk_type, seq_type, input);
  }

  AtomBuffer() = delete;
  AtomBuffer(const AtomBuffer&) = default;
  AtomBuffer(AtomBuffer&&) = default;
  AtomBuffer& operator=(const AtomBuffer&) = default;
  AtomBuffer& operator=(AtomBuffer&&) = default;
  ~AtomBuffer() { ::operator delete(buf); }
};

#endif // LV2_ATOM_HELPERS_H

// end of lv2_atom_helpers.h
