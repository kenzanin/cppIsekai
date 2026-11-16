-- Text helpers shared by the 1manga scrapers (React comment decoding, tag/status
-- canonical forms). Pure functions; imported with require("text").
local M = {}

-- React leaves <!-- --> between interpolated fragments; drop them so the
-- remaining text can be matched with plain Lua patterns.
function M.decomment(s)
  return (tostring(s or ""):gsub("<!%-%-.-%-%->", ""))
end

function M.unesc(s)
  return host.text.unescape(host.text.trim(tostring(s or "")))
end

function M.strip_tags(s)
  return M.unesc((tostring(s or ""):gsub("<[^>]->", "")))
end

-- "12-08-2025" (M-D-Y, the site's chapter date) -> "2025-12-08".
function M.iso_date(raw)
  local mo, d, y = string.match(tostring(raw or ""), "^(%d%d?)-(%d%d?)-(%d%d%d%d)$")
  if not mo then return "" end
  return string.format("%04d-%02d-%02d", tonumber(y), tonumber(mo), tonumber(d))
end

-- Plugin-local normalizers (Go-port style). The host stays the source of
-- truth; we feed it already-canonical values: status is lowercase canonical,
-- tags are lowercase trimmed/collapsed/deduped. Same shape the store folds.
function M.normalize_status(s)
  s = tostring(s or ""):lower():gsub("[^a-z0-9]", "")
  if s == "" then return "unknown" end
  if s == "ongoing" or s == "ongoin" or s == "releasing" or s == "publishing" then return "ongoing" end
  if s == "completed" or s == "finished" then return "completed" end
  if s == "hiatus" or s == "onhaiatus" then return "hiatus" end
  if s == "dropped" or s == "cancelled" or s == "canceled" then return "dropped" end
  if s == "upcoming" or s == "notyetreleased" then return "upcoming" end
  return s
end

function M.normalize_tag(s)
  s = tostring(s or "")
  s = s:gsub("[_%s]+", " ")
  s = s:gsub("^%s+", ""):gsub("%s+$", "")
  return s:lower()
end

-- Site chapter labels include scrape noise: "CHapter 3860" and "3854-eng-li".
-- Drop those (the host renders the number itself); keep real titles verbatim.
function M.clean_title(s)
  if s == "" then return "" end
  if string.match(s, "^%a+apter%s*[%d%.]+$") then return "" end
  if string.match(s, "^[%d%.]+%-%a+%-%a+$") then return "" end
  return s
end

-- longest <p> body on the page that carries prose (detail synopsis); the genre
-- row is a <p> of anchors and loses on length plus the <a> test.
function M.longest_paragraph(html)
  local best = ""
  for body in string.gmatch(html or "", "<p[^>]->(.-)</p>") do
    if not body:find("<a ", 1, true) then
      local text = M.strip_tags(body)
      if #text > #best then best = text end
    end
  end
  return (#best >= 60) and best or ""
end

return M
