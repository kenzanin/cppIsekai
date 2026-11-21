#include "plugin/lua/bindings.hpp"

#include <filesystem>

namespace plugin::lua {

void bind_fs(lua_State* L, sol::table host, const HostCtx& ctx) {
    sol::state_view sv(L);
    sol::table fs = sv.create_table();
    fs["read"] = [dir = ctx.dir](const std::string& rel) -> std::optional<std::string> { return fs_read(dir, rel); };
    // Shared read-only lib root: <data>/plugins_lib -> plugins_dev/lib (two
    // levels above the per-plugin dir). require()'s fallback, never written by
    // plugins. plugin-host-promotion D8.
    fs["read_lib"] = [dir = ctx.dir](const std::string& rel) -> std::optional<std::string> {
        const std::filesystem::path root = std::filesystem::path(dir).parent_path().parent_path() / "plugins_lib";
        return fs_read(root, rel);
    };
    host["fs"] = fs;

    // Confined require() as a pure-Lua helper on top of host.fs.read:
    // no C++ stack/object lifetime to get wrong across state teardown.
    // Resolves "a.b" -> <dir>/a/b.lua (or a/b/init.lua); per-state cache in a
    // hidden global; path segments are checked for traversal in Lua.
    sv.script(R"lua(
local fs_read = host.fs.read
local fs_read_lib = host.fs.read_lib
local cache = {}
local function fail(msg) error("require: " .. msg, 0) end
function require(name)
  if type(name) ~= "string" or name == "" then fail("empty module name") end
  local hit = cache[name]
  if hit ~= nil then return hit end
  local rel = (name:gsub("%.", "/"))
  if rel:sub(1, 1) == "/" then fail("bad module name '" .. name .. "'") end
  for seg in rel:gmatch("[^/]+") do
    if seg == "." or seg == ".." then fail("bad module name '" .. name .. "'") end
  end
  local src, chunkname = fs_read(rel .. ".lua"), "@" .. rel .. ".lua"
  if src == nil then
    src, chunkname = fs_read(rel .. "/init.lua"), "@" .. rel .. "/init.lua"
  end
  if src == nil and fs_read_lib then -- shared lib fallback (read-only)
    src, chunkname = fs_read_lib(rel .. ".lua"), "@lib/" .. rel .. ".lua"
    if src == nil then
      src, chunkname = fs_read_lib(rel .. "/init.lua"), "@lib/" .. rel .. "/init.lua"
    end
  end
  if src == nil then fail("module '" .. name .. "' not found") end
  local chunk, err = load(src, chunkname)
  if chunk == nil then fail(tostring(err)) end
  local ok, result = pcall(chunk)
  if not ok then fail(tostring(result)) end
  if result == nil then result = true end
  cache[name] = result
  return result
end
)lua");
}

} // namespace plugin::lua
