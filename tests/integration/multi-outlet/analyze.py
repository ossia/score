#!/usr/bin/env python3
"""Assertions for the multi-outlet grid grab (see multi-outlet.js).

  analyze.py <grid.png> <image> <model>

The grab is a 2x2 grid: source | IP Image / IP Mask | IP Depth. The Image
Processor tiles are compared against onnxruntime references computed from the
same photo and model: the Mask tile must follow the sky output and the Depth
tile the depth output. Before the score fix every CPU texture outlet drew the
first one, so Mask and Depth showed the (unwritten, black) Image outlet.

Structure is compared (Pearson correlation after downsampling), not pixels: the
Images process framing and the render path are not byte-identical to the
reference preprocessing. Both vertical orientations are tried, since texture
origin conventions differ between backends.
"""
import sys
import numpy as np
from PIL import Image

W, H = 126, 70  # comparison size, ~ the 504x280 model aspect


def tiles(path):
    im = np.asarray(Image.open(path).convert("RGB")).astype(np.float32) / 255.0
    h, w, _ = im.shape
    out = {}
    for name, (y, x) in {"tl": (0, 0), "tr": (0, 1), "bl": (1, 0), "br": (1, 1)}.items():
        t = im[y * h // 2:(y + 1) * h // 2, x * w // 2:(x + 1) * w // 2]
        # Skip the grid gap and edge seams.
        th, tw, _ = t.shape
        t = t[th // 20: th - th // 20, tw // 20: tw - tw // 20]
        out[name] = t
    return out


def small(a):
    a = np.asarray(a, np.float32)
    return np.asarray(Image.fromarray(a).resize((W, H), Image.BILINEAR), np.float32)


def corr(a, b):
    a = a.ravel() - a.mean()
    b = b.ravel() - b.mean()
    d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d > 0 else 0.0


def best_corr(tile, ref):
    return max(corr(tile, ref), corr(tile[::-1], ref))


def references(image, model):
    import onnxruntime as ort
    so = ort.SessionOptions()
    so.log_severity_level = 3
    s = ort.InferenceSession(model, so, providers=["CPUExecutionProvider"])
    i = s.get_inputs()[0]
    mh, mw = i.shape[2], i.shape[3]
    # The Images process stretches the photo into the processor's 512x512
    # input, then the processor's "Crop" keeps aspect, fills, center-crops.
    img = Image.open(image).convert("RGB").resize((512, 512), Image.BILINEAR)
    iw, ih = img.size
    sc = max(mw / iw, mh / ih)
    img = img.resize((round(iw * sc), round(ih * sc)), Image.BILINEAR)
    l, t = (img.size[0] - mw) // 2, (img.size[1] - mh) // 2
    img = img.crop((l, t, l + mw, t + mh))
    x = (np.asarray(img, np.float32) / 255.0 - [0.485, 0.456, 0.406]) / [0.229, 0.224, 0.225]
    x = x.transpose(2, 0, 1)[None].astype(np.float32)
    depth, sky = s.run(None, {i.name: x})
    # The Depth outlet is raw R32F meters, but it reaches the Grid through the
    # ISF input's 8-bit render target, which clamps it to [0, 1] before the
    # Grid's gain. Compare against what can actually arrive.
    return small(np.clip(depth[0, 0], 0.0, 1.0)), small(sky[0, 0])


def main():
    grid, image, model = sys.argv[1:4]
    t = tiles(grid)
    ref_depth, ref_sky = references(image, model)

    fails = []

    def check(cond, msg):
        print(("  ok   " if cond else "  FAIL ") + msg)
        if not cond:
            fails.append(msg)

    tl, tr, bl, br = t["tl"], t["tr"], t["bl"], t["br"]
    sat = lambda a: float((a.max(axis=2) - a.min(axis=2)).mean())

    print(f"tile means: tl={tl.mean():.3f} tr={tr.mean():.3f} bl={bl.mean():.3f} br={br.mean():.3f}")
    check(tl.mean() > 0.1 and sat(tl) > 0.05, f"source tile is a colour picture (mean {tl.mean():.3f}, saturation {sat(tl):.3f})")
    check(tr.max() < 0.05, f"IP Image outlet (not written by this model) is black (max {tr.max():.3f})")

    for name, a in (("Mask", bl), ("Depth", br)):
        r, g, b = a[..., 0], a[..., 1], a[..., 2]
        check(max(g.mean(), b.mean()) < 0.02, f"IP {name} outlet is single-channel (g {g.mean():.3f}, b {b.mean():.3f})")
    check(bl[..., 0].std() > 0.05, f"IP Mask outlet has content (red std {bl[..., 0].std():.3f})")
    # Depth arrives clamped to [0, 1] (see references()): in this scene nearly
    # everything is farther than 1 m, so the tile is mostly saturated and only
    # the near structure survives. Assert it is written, not its spread.
    check(br[..., 0].mean() > 0.1, f"IP Depth outlet is written (red mean {br[..., 0].mean():.3f})")

    blr, brr = small(bl[..., 0]), small(br[..., 0])
    diff = float(np.abs(blr / max(blr.max(), 1e-6) - brr / max(brr.max(), 1e-6)).mean())
    check(diff > 0.05, f"Mask and Depth outlets differ (mean normalized abs diff {diff:.3f})")

    m_sky, m_depth = best_corr(blr, ref_sky), best_corr(blr, ref_depth)
    d_sky, d_depth = best_corr(brr, ref_sky), best_corr(brr, ref_depth)
    print(f"correlations: Mask~sky {m_sky:.3f} Mask~depth {m_depth:.3f} | Depth~depth {d_depth:.3f} Depth~sky {d_sky:.3f}")
    check(m_sky > 0.6, f"Mask outlet follows the sky output (r={m_sky:.3f})")
    check(d_depth > 0.5, f"Depth outlet follows the [0,1]-clamped depth output (r={d_depth:.3f})")
    check(m_sky > m_depth and d_depth > d_sky,
          "each outlet matches its own model output better than the other one")

    print("ANALYZE " + ("PASS" if not fails else f"FAIL ({len(fails)})"))
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
