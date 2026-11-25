-- Shared "get it" fetcher: the one place that carries the UA and timeout.
-- Returns (body, nil) / (nil, error-string).
local M = {}

local UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"

function M.get(url)
  local res, err = host.http.fetch(url, { headers = { ["User-Agent"] = UA }, timeout_ms = 30000 })
  if err ~= nil then
    return nil, err
  end
  if res.status ~= 200 then
    return nil, "1manga: upstream status " .. tostring(res.status)
  end
  return res.body, nil
end

return M
