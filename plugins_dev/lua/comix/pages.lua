local c = require("common")
local BASE = c.BASE
local render, find1, cover_abs = c.render, c.find1, c.cover_abs
local initial_data, relative_to_iso = c.initial_data, c.relative_to_iso
-- ── pages ──────────────────────────────────────────────────────────────────

-- The page list exists only in the rendered reader: the chapter API returns
-- an encrypted payload the site JS decrypts, and img srcs appear lazily via
-- IntersectionObserver. host.browser (change browser-host) runs the page's
-- own JS: goto, then ONE evaluate whose promise scrolls every .rpage-page
-- slot and collects the img src it materialized (lightpanda awaits promises;
-- verified: 6/6 urls, ~0.9s, image/webp without Referer).
local HARVEST =
  '(function(){return new Promise(function(res){var t0=Date.now();function urls(){return Array.prototype.slice.call(document.querySelectorAll(".rpage-page img")).map(function(im){return im.getAttribute("src");}).filter(Boolean);}function done(){res(JSON.stringify(urls()));}function scroll(slots,i){if(i<slots.length){slots[i].scrollIntoView();setTimeout(function(){scroll(slots,i+1);},150);}else{done();}}function wait(){var slots=document.querySelectorAll(".rpage-page");if(slots.length)return scroll(slots,0);if(Date.now()-t0>8000)return done();setTimeout(wait,100);}wait();});})()'

return function(params)
  local composite = params.id or ""
  local slug, chid = string.match(composite, "^([^:]+):(.+)$")
  if not slug or not chid or slug == "" then
    return { ok = false, error = "comix: chapter id must be slug:chapterId, got " .. tostring(composite) }
  end
  local _, gerr = host.browser.call(
    "goto",
    { url = BASE .. "/title/" .. host.url_encode(slug) .. "/" .. host.url_encode(chid), timeout_ms = 60000 }
  )
  if gerr ~= nil then
    return { ok = false, error = "comix: goto failed: " .. gerr }
  end
  local out, eerr = host.browser.call("evaluate", { script = HARVEST, timeout_ms = 60000 })
  if eerr ~= nil then
    return { ok = false, error = "comix: harvest failed: " .. eerr }
  end
  local ok, urls = pcall(json.decode, out or "")
  if not ok or type(urls) ~= "table" then
    return { ok = false, error = "comix: unexpected harvest payload for " .. composite }
  end
  if #urls == 0 then
    host.log.error("comix: no page images in chapter: " .. composite)
    return { ok = false, error = "comix: no pages for " .. composite }
  end
  host.log.info("comix: found " .. tostring(#urls) .. " pages for " .. composite)
  return { ok = true, data = urls }
end
