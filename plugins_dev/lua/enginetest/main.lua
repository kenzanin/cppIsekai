-- throwaway probe: mangabuddy api search pagination shape
local function probe(p)
  local out = {}
  for _, pg in ipairs({ 1, 2 }) do
    local url = "https://mangabuddy1.co.uk/api/search?search=dungeon&page=" .. pg
    local res = host.http.fetch(url, {})
    local t = { status = res and res.status or 0 }
    if res and res.status == 200 then
      local ok, data = pcall(json.decode, res.body or "")
      if ok and type(data) == "table" then
        local cs = data.comics or {}
        local ids = {}
        for i = 1, math.min(3, #cs) do
          ids[i] = tostring(cs[i].slug_hash or "?")
        end
        t.n = #cs
        t.first = table.concat(ids, ",")
      else
        t.err = "json"
      end
    else
      t.err = "http " .. tostring(t.status)
    end
    out["p" .. pg] = t
  end
  return { ok = true, data = out }
end
function handle(req)
  return probe(req.params or {})
end
