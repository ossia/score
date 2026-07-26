// Open a score document named by the page URL: score-web/?open=<path-or-URL>
//
// Only same-origin documents are accepted. A .score document can carry scripts,
// device definitions and network addresses, so fetching one from an arbitrary
// host on a visitor's behalf is not something the app may do; and since the
// documentation (ossia.io/score-docs) and the app (ossia.io/score-web) share an
// origin, every documentation link is expressible as an origin-relative path.
//
// The target may be a bare document, or a zip archive holding one document at
// its root next to the media it uses: score resolves "<PROJECT>:<file>" media
// references against the directory the document sits in, so extracting the
// archive into a directory of its own makes those references resolve.
//
// Everything is written into MEMFS during preRun -- i.e. before main() runs --
// and the document path is passed as argv[1], so that it goes through score's
// ordinary "file given on the command line" path and no boot-order hook is
// needed.

const SCORE_IMPORTS_DIR = '/score/imports';
const SCORE_OPEN_MAX_BYTES = 256 * 1024 * 1024;
const SCORE_OPEN_MAX_EXTRACTED_BYTES = 512 * 1024 * 1024;
const SCORE_DOCUMENT_RE = /\.(score|scorebin|scorejson)$/;

// Returns the URL to open, or null when the page was not asked to open one.
// Throws Error with a user-facing message when the parameter is unusable.
function scoreOpenUrlTarget(location) {
  const raw = new URLSearchParams(location.search).get('open');
  if (!raw)
    return null;

  let url;
  try {
    url = new URL(raw, location.href);
  } catch (e) {
    throw new Error(`"${raw}" is not a valid document URL.`);
  }

  if (url.protocol !== 'http:' && url.protocol !== 'https:')
    throw new Error(`"${url.protocol}" documents cannot be opened.`);

  if (url.origin !== location.origin) {
    throw new Error(
      `documents can only be opened from ${location.origin}, not from ${url.origin}. ` +
      `Download it and open it from the File menu instead.`);
  }

  return url;
}

function scoreOpenUrlBaseName(url) {
  let name;
  try {
    name = decodeURIComponent(url.pathname.split('/').pop() || '');
  } catch (e) {
    name = '';
  }
  return name.replace(/[^A-Za-z0-9._-]/g, '_');
}

function scoreOpenUrlFileName(url) {
  const name = scoreOpenUrlBaseName(url);
  return SCORE_DOCUMENT_RE.test(name) ? name : (name || 'document') + '.score';
}

function scoreOpenUrlIsZip(bytes) {
  return bytes.length >= 4 && bytes[0] === 0x50 && bytes[1] === 0x4b
      && bytes[2] === 0x03 && bytes[3] === 0x04;
}

// Rejects entry names that would escape the extraction directory (absolute
// paths, drive letters, "..", NUL) and directory entries. Returns null for
// anything that must not be written.
function scoreZipSafeName(name) {
  const normalized = name.replace(/\\/g, '/');
  if (normalized === '' || normalized.endsWith('/'))
    return null;
  if (normalized.startsWith('/') || /^[A-Za-z]:/.test(normalized))
    return null;
  if (normalized.includes('\0'))
    return null;
  const parts = normalized.split('/');
  if (parts.some(p => p === '' || p === '.' || p === '..'))
    return null;
  return normalized;
}

// Minimal zip reader: central directory only, methods 0 (stored) and 8
// (deflate). Enough for the archives the documentation publishes, and avoids
// bundling a zip library into the deployment.
function scoreZipCentralDirectory(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const u16 = o => view.getUint16(o, true);
  const u32 = o => view.getUint32(o, true);

  // The end-of-central-directory record is last, but a trailing comment of up
  // to 64k may follow it, so it has to be searched for backwards.
  let eocd = -1;
  const floor = Math.max(0, bytes.length - 22 - 0xffff);
  for (let i = bytes.length - 22; i >= floor; i--) {
    if (u32(i) === 0x06054b50) { eocd = i; break; }
  }
  if (eocd < 0)
    throw new Error('the archive has no end-of-central-directory record.');

  const count = u16(eocd + 10);
  let offset = u32(eocd + 16);
  if (count === 0xffff || offset === 0xffffffff)
    throw new Error('zip64 archives are not supported.');

  const decoder = new TextDecoder();
  const entries = [];
  for (let i = 0; i < count; i++) {
    if (offset + 46 > bytes.length || u32(offset) !== 0x02014b50)
      throw new Error('the archive central directory is corrupt.');

    const method = u16(offset + 10);
    const compressedSize = u32(offset + 20);
    const uncompressedSize = u32(offset + 24);
    const nameLen = u16(offset + 28);
    const extraLen = u16(offset + 30);
    const commentLen = u16(offset + 32);
    const localOffset = u32(offset + 42);
    const rawName = decoder.decode(bytes.subarray(offset + 46, offset + 46 + nameLen));

    entries.push({rawName, method, compressedSize, uncompressedSize, localOffset});
    offset += 46 + nameLen + extraLen + commentLen;
  }
  return entries;
}

async function scoreZipInflate(raw) {
  const stream = new Blob([raw]).stream()
    .pipeThrough(new DecompressionStream('deflate-raw'));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

// Extracts into `dir`, returning [{path, bytes}] for every file kept.
async function scoreZipExtract(bytes, dir) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const u16 = o => view.getUint16(o, true);
  const u32 = o => view.getUint32(o, true);

  const files = [];
  let extracted = 0;
  for (const entry of scoreZipCentralDirectory(bytes)) {
    const name = scoreZipSafeName(entry.rawName);
    if (!name)
      continue;

    extracted += entry.uncompressedSize;
    if (extracted > SCORE_OPEN_MAX_EXTRACTED_BYTES)
      throw new Error('the archive expands to more than the allowed size.');

    const local = entry.localOffset;
    if (local + 30 > bytes.length || u32(local) !== 0x04034b50)
      throw new Error(`the archive entry "${name}" is corrupt.`);
    const start = local + 30 + u16(local + 26) + u16(local + 28);
    const raw = bytes.subarray(start, start + entry.compressedSize);

    let content;
    if (entry.method === 0)
      content = raw;
    else if (entry.method === 8)
      content = await scoreZipInflate(raw);
    else
      throw new Error(`the archive entry "${name}" uses an unsupported compression method.`);

    files.push({path: `${dir}/${name}`, bytes: content});
  }
  return files;
}

// Exactly one document at the root of the archive is the expected shape: it is
// what identifies the project, and score resolves media relative to it.
function scoreZipRootDocument(files, dir) {
  const roots = files
    .map(f => f.path)
    .filter(p => p.startsWith(dir + '/')
                 && !p.slice(dir.length + 1).includes('/')
                 && SCORE_DOCUMENT_RE.test(p));

  if (roots.length === 0)
    throw new Error('the archive contains no score document at its root.');
  if (roots.length > 1) {
    const names = roots.map(p => p.slice(dir.length + 1)).join(', ');
    throw new Error(
      `the archive contains ${roots.length} score documents at its root (${names}); `
      + `it must contain exactly one.`);
  }
  return roots[0];
}

// Returns {path, files} -- the document to open and everything to write into
// MEMFS -- or null when the page was not asked to open anything.
async function scoreOpenUrlFetch(location) {
  const url = scoreOpenUrlTarget(location);
  if (!url)
    return null;

  let response;
  try {
    response = await fetch(url, {credentials: 'same-origin'});
  } catch (e) {
    throw new Error(`${url.pathname} could not be fetched (${e.message}).`);
  }
  if (!response.ok)
    throw new Error(`${url.pathname} could not be fetched (HTTP ${response.status}).`);

  const buffer = await response.arrayBuffer();
  if (buffer.byteLength === 0)
    throw new Error(`${url.pathname} is empty.`);
  if (buffer.byteLength > SCORE_OPEN_MAX_BYTES)
    throw new Error(`${url.pathname} is too large (${buffer.byteLength} bytes).`);

  const bytes = new Uint8Array(buffer);
  const base = scoreOpenUrlBaseName(url);

  if (scoreOpenUrlIsZip(bytes)) {
    const dir = `${SCORE_IMPORTS_DIR}/${base.replace(/\.zip$/i, '') || 'project'}`;
    const files = await scoreZipExtract(bytes, dir);
    return {path: scoreZipRootDocument(files, dir), files};
  }

  // The magic is authoritative, so say so rather than handing score a zip.
  if (/\.zip$/i.test(base))
    throw new Error(`${url.pathname} is not a zip archive.`);

  const path = `${SCORE_IMPORTS_DIR}/${scoreOpenUrlFileName(url)}`;
  return {path, files: [{path, bytes}]};
}

// preRun callback: emscripten passes the Module as first argument.
function scoreOpenUrlStage(module, doc) {
  const FS = module.FS;
  for (const file of doc.files) {
    FS.mkdirTree(file.path.slice(0, file.path.lastIndexOf('/')));
    FS.writeFile(file.path, file.bytes);
  }
}

// Errors here happen before score is up, so they cannot be reported by score
// itself: show them in the page, over the application, until dismissed.
function scoreOpenUrlShowError(message) {
  const bar = document.createElement('div');
  bar.id = 'score-open-error';
  // Anchored at the bottom: the application's menu bar is at the top.
  bar.style.cssText = 'position:fixed; z-index:10000; bottom:0; left:0; right:0;'
    + ' padding:0.7em 1em; font:14px sans-serif; color:#fff; background:#8b1a1a;'
    + ' box-shadow:0 -2px 6px rgba(0,0,0,0.4); display:flex; gap:1em;'
    + ' align-items:baseline;';

  const text = document.createElement('span');
  text.style.cssText = 'flex:1';
  text.textContent = 'Could not open the requested document: ' + message;

  const close = document.createElement('button');
  close.textContent = 'Dismiss';
  close.onclick = () => bar.remove();

  bar.appendChild(text);
  bar.appendChild(close);
  document.body.appendChild(bar);
}

// Adds `arguments` and `preRun` entries to an emscripten module config so that
// the document named by the page URL, if any, is opened at startup. Never
// throws: a document that cannot be fetched is reported and the app boots empty.
async function scoreOpenUrlConfigure(config) {
  let doc = null;
  try {
    doc = await scoreOpenUrlFetch(window.location);
  } catch (e) {
    console.error('score: ' + e.message);
    scoreOpenUrlShowError(e.message);
    return config;
  }

  if (!doc)
    return config;

  config.arguments = (config.arguments ?? []).concat([doc.path]);
  config.preRun = (config.preRun ?? []).concat([(module) => scoreOpenUrlStage(module, doc)]);
  window.scoreOpenedDocument = doc.path;
  return config;
}
