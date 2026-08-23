#!/usr/bin/env node
//
// Extracts the ExprTK presets that ship in the score user library
// (https://github.com/ossia/score-user-library, "default" package) into a C++
// header, so that tests/unit/ExpressionPresetsTest.cpp can replay every single
// one of them through the real nodes.
//
// The .scp files are the source of truth; this header is derived from them.
// Regenerate after adding or editing a preset:
//
//   node tests/unit/tools/gen-expression-preset-corpus.js \
//        ~/Documents/ossia/score/packages/default/Presets \
//        tests/unit/ExpressionPresetCorpus.hpp
//
'use strict';
const fs = require('fs');
const path = require('path');

const [, , PRESETS_ROOT, OUT] = process.argv;
if (!PRESETS_ROOT || !OUT) {
  console.error('usage: gen-expression-preset-corpus.js <presets-root> <out.hpp>');
  process.exit(1);
}

// Presets are dispatched on the process uuid, not on the folder they sit in:
// a preset can be filed anywhere in the library (Expression Value Generator
// presets live in both "Expression Value Generator" and "Value Generator").
const PROCESSES = [
  { uuid: 'cf3df02f-a563-4e92-a739-b321d3a84252', name: 'Arraygen',
    sym: 'arraygen', expr: 0, size: 1 },
  { uuid: '1fe9c806-b601-4ee0-9fbb-0ab817c4dd87', name: 'Arraymap',
    sym: 'arraymap', expr: 1 },
  { uuid: '25c64b87-a44a-4fed-9f60-0a48906fd3ec', name: 'Micromap',
    sym: 'micromap', expr: 1 },
  { uuid: 'ae84e8b6-74ff-4259-aeeb-305d95cdfcab', name: 'Expression Value Filter',
    sym: 'value_filter', expr: 1, params: [2, 3, 4] },
  { uuid: 'd757bd0d-c0a1-4aec-bf72-945b722ab85b', name: 'Expression Value Generator',
    sym: 'value_generator', expr: 0, params: [1, 2, 3] },
  { uuid: '13e1f4b0-1c2c-40e6-93ad-dfc91aac5335', name: 'Expression Audio Filter',
    sym: 'audio_filter', expr: 1, params: [2, 3, 4] },
  { uuid: 'eae294b3-afeb-4fba-bbe4-337998d3748a', name: 'Expression Audio Generator',
    sym: 'audio_generator', expr: 0, params: [1, 2, 3] },
];

function walk(dir, out = []) {
  for(const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if(e.isDirectory())
      walk(p, out);
    else if(e.name.endsWith('.scp'))
      out.push(p);
  }
  return out;
}

// JSON string escapes are a subset of C++ ones, so this doubles as a C++
// literal - as long as everything stays ASCII.
function cxx(s) {
  if (/[^\x20-\x7e\n\r\t]/.test(s))
    throw new Error('non-ASCII character in preset: ' + JSON.stringify(s));
  return JSON.stringify(s);
}

const ALL_FILES = walk(PRESETS_ROOT).sort();

function entriesFor(proc) {
  const out = [];
  for (const file of ALL_FILES) {
    const j = JSON.parse(fs.readFileSync(file, 'utf8'));
    if (!j.Key || j.Key.Uuid !== proc.uuid)
      continue;

    const ports = new Map((j.Preset || []).map(([id, v]) => [id, v]));
    const expr = ports.get(proc.expr);
    if (!expr || typeof expr.String !== 'string')
      throw new Error(`${file}: no expression at port ${proc.expr}`);

    const abc = (proc.params || []).map(id => {
      const v = ports.get(id);
      return v && v.Float !== undefined ? v.Float : 0.5;
    });
    while (abc.length < 3) abc.push(0.5);

    const sizePort = proc.size !== undefined ? ports.get(proc.size) : undefined;
    const size = sizePort && sizePort.Int !== undefined ? sizePort.Int : 12;

    out.push({
      name: j.Name || path.basename(file, '.scp'),
      category: j.Category || '',
      expr: expr.String,
      abc,
      size,
    });
  }
  out.sort((l, r) => (l.name < r.name ? -1 : l.name > r.name ? 1 : 0));
  return out;
}

const lines = [];
lines.push('#pragma once');
lines.push('// GENERATED FILE - do not edit by hand.');
lines.push('//');
lines.push('// Every ExprTK preset shipped in the score user library ("default" package),');
lines.push('// extracted from its .scp file. Regenerate with:');
lines.push('//   node tests/unit/tools/gen-expression-preset-corpus.js \\');
lines.push('//        <score-user-library>/default/Presets tests/unit/ExpressionPresetCorpus.hpp');
lines.push('');
lines.push('namespace preset_corpus');
lines.push('{');
lines.push('//! One preset: the expression plus the control values stored with it.');
lines.push('struct entry');
lines.push('{');
lines.push('  const char* name;');
lines.push('  const char* category; //!< menu path, "/" nests submenus');
lines.push('  const char* expr;');
lines.push('  float a, b, c;');
lines.push('  int size; //!< Arraygen only');
lines.push('};');
lines.push('');

const num = v => {
  const s = String(v);
  return (s.includes('.') || s.includes('e') ? s : s + '.') + 'f';
};

let total = 0;
for (const proc of PROCESSES) {
  const es = entriesFor(proc);
  total += es.length;
  lines.push(`// ${proc.name} (${es.length})`);
  lines.push(`inline constexpr entry ${proc.sym}[]{`);
  for (const e of es) {
    lines.push(`    {${cxx(e.name)}, ${cxx(e.category)},`);
    lines.push(`     ${cxx(e.expr)},`);
    lines.push(`     ${num(e.abc[0])}, ${num(e.abc[1])}, ${num(e.abc[2])}, ${e.size}},`);
  }
  lines.push('};');
  lines.push('');
}
lines.push('}');

fs.mkdirSync(path.dirname(OUT), { recursive: true });
fs.writeFileSync(OUT, lines.join('\n') + '\n');
console.error(`wrote ${OUT}: ${total} presets`);
