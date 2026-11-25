-- comizy.io: Next.js manga site, API-first (api.comizy.io + /_next/data
-- buildId routes). Port of goIsekai examples/plugins/lua/comizy to the
-- cppIsekai host ABI (host.http.fetch + status check, json globals).
-- Pages are plain URL strings; Referer comes from the host image-proxy
-- fallback to site_url, same as the source relied on.
local API = "https://api.comizy.io" -- single-domain pair; main.json params kept verbatim, unused
local WEB = "https://comizy.io"

-- GET + 200 + JSON decode -> table, or nil (goIsekai get_json semantics).
local function get_json(url)
  return (host.http.get_json(url, { headers = { accept = "application/json" } }))
end

-- buildId rotates per deploy; cache per VM lifetime. genres cache + slug map
-- live at file scope so every method shares them.
local cached_build_id = nil
local cached_genres, name_to_slug = nil, {}
local function build_id()
  if cached_build_id then return cached_build_id end
  local v = get_json(WEB .. "/api/version")
  if v and v.buildId then cached_build_id = v.buildId end
  return cached_build_id
end

local function split_id(id)
  id = tostring(id or "")
  local sep = id:find(".", 1, true)
  if not sep then return nil, nil end
  return id:sub(1, sep - 1), id:sub(sep + 1)
end

-- comizy labels a split chapter "Chapter 12-2" and a decimal one "Chapter
-- 12.1"; chapter_num reads the leading number only, so the hyphen form
-- collapsed to 12 and collided with its sibling. Fold hyphen -> dot.
local function comizy_chapter_num(title)
  local t = (tostring(title or ""):gsub("^(%s*[Cc]hapter%s+%d+)%s*%-%s*(%d+)%s*$", "%1.%2"))
  return host.text.chapter_num(t)
end


-- [{id,slug,name,cover,...}] -> canonical search items, id packed.
local function items_from(raw)
  local items = {}
  for _, it in ipairs(raw) do
    if it.id and it.slug then
      items[#items + 1] = {
        id = tostring(it.id) .. "." .. tostring(it.slug),
        name = it.name or it.slug,
        cover = it.cover or "",
      }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── search ─────────────────────────────────────────────────────────────────
-- id packs "{mangaId}.{slug}" (split at first dot; both halves have no dots): chapters need the opaque mangaId, pages need
-- the slug — neither derives the other. Genre mode browses the SSR JSON
-- (the REST search silently ignores genre params); query is ignored there.



return { API=API, WEB=WEB, get_json=get_json, cached_build_id=cached_build_id, cached_genres=cached_genres, name_to_slug=name_to_slug, build_id=build_id, split_id=split_id, comizy_chapter_num=comizy_chapter_num, items_from=items_from }
