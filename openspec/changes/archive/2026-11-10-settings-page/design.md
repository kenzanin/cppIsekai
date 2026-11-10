# Design

## Context
`POST /config` already implements validation-first merge-save (registry keys only, numeric types checked), and it reloads `plugin.fetch_profile` live. There is no read projection. Settings page follows the codebase's mustache + Alpine pattern (Plugins page is the closest model: server-rendered shell + Alpine fetch), not a new SPA.

## Decisions
- **D1 `GET /config` projects the registry, not the ini.** Return per knob: `section`, `key`, `fullKey` (`section.key`), `doc`, `type`, `flag`, `env`, `default`, `value` (resolved layered value for the running process's view: default < ini < env; explicit CLI flag is an override the user set on the command line and is shown as the value). Implementation: one helper in `src/config/` (e.g. `config::describe()`) returning a glaze-friendly struct array; `register_api` renders it. No config logic outside `src/config/`.
- **D2 Edit path is the existing `POST /config`, unchanged.** Body `{"section.key": v}`; the page collects dirty fields only and posts them. `{ok, written}` vs `{ok:false, error}` surfaces verbatim on the page (same `err` line pattern as other pages).
- **D3 Resolution semantics surfaced in the UI, not hidden.** Each row shows which layer won (default / ini / env / flag) — computed by the same helper. A field whose winner is `flag`/`env` renders read-only with a hint ("set by --flag / CPPISEKAI_* — editing the ini won't change it"). This is the honest version of "why didn't my edit apply".
- **D4 Restart hints, not auto-restart.** Registry gains a `restart` hint per knob (server.port, paths.*, cache.cache_max_mb, cache.image_format, cache.cover_max_px, log.level). Only `plugin.fetch_profile` is live (existing behavior). Hint text rendered once per section, plus a per-row badge where applicable.
- **D5 Enum inputs for typed-ish strings.** `image_format` (webp|avif|jxl|none), `log.level` (trace|debug|info|warning|error), `fetch_profile` (free text with an autocomplete-ish hint list of known targets from `fetch::is_known_target`) render as `<select>`; everything else text/number input. No client-side validation beyond required/numeric — server is the validator.
- **D6 Secrets are not a thing here** — no knob holds credentials today (proxy URL could embed one; treat `server.proxy` as password field with reveal toggle, value still readable server-side as before).
- **D7 Nav: `Settings` after `Plugins`.** Link to `/settings`.

## Goals / Non-Goals
**Goals:** view every registry knob with layer provenance; edit ini-backed knobs with comments/ordering preserved; honest restart/live hints; failure modes visible.
**Non-Goals:** adding/removing knobs from the UI; editing env/flag sources; ini syntax editing; daemon restart button (D4); per-plugin settings; authn/z (single-user local daemon, same exposure as today's POST /config).

## Risks / Trade-offs
- [Resolved value shown vs. value-after-save diverge when env/flag wins] → D3 read-only + hint; save still writes ini (honest merge) and reports written count; page re-GETs after save and reports what actually resolved.
- [`cache.image_format`/`cover_max_px` apply to new fetches only; cache contents unaffected] → hint text on those rows.
- [`cache_max_mb` is read at boot into `cache::Config`] → restart hint.
- [A bad GET helper leaks layering internals?] → it's the documented registry, already in `--dump-config` output.
