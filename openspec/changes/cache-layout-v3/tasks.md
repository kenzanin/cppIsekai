# Tasks

## 1. Config + transcode core

- [ ] 1.1 Registry knobs `[cache] image_format` (webp|avif|jxl|none, default webp) + `cover_max_px` (default 800) in `src/config/`; `--dump-config` shows both; unit test round-trips env/flag
- [ ] 1.2 `src/cache/transcode.hpp` (header-only): vips load-from-buffer → encode to target ext, cover resize (fit `cover_max_px`, keep aspect) → returns bytes+ext+w·h; decode failure = passthrough; unit test with tiny generated buffers (webp in → webp out, junk in → passthrough)
- [ ] 2.1 `img_store(bytes, ctx)` context-aware rel_path builder (cover / ch<n>-<id8>/pNNN / hash fallback); cover sidecar `<hash>.json` {w,h,ratio} from decoded dims (plugin hint only on decode failure); extension-free lookup by key on serve; unit tests for path shapes + duplicate chapter disambiguation
- [ ] 2.2 `/cache/img` route: optional `plugin,slug,ch,page,cover` params (no ratio — ignored if passed); `X-Image-Ratio` header from sidecar on cover serves; reader worker + detail/chapters templates pass context; cbz builder unchanged (walks slug dir, sorted pNNN)

## 3. Verify

- [ ] 3.1 Live smoke: fetch cover+page via curl with context → files land in v3 paths in configured format; flip `image_format` mid-run → old files still serve; full ctest green, zero new warnings; `openspec validate --specs`
