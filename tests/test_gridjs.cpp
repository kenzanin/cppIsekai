#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <quickjs.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Extract the single <script>...</script> block (the gridPage component).
std::string extract_script(const std::string& page) {
    const auto open = page.find("<script>");
    const auto close = page.find("</script>", open);
    if (open == std::string::npos || close == std::string::npos) {
        throw std::runtime_error("no <script> block in grid template");
    }
    return page.substr(open + 8, close - open - 8);
}

// Minimal browser shim: just the surface grid_body's component touches
// (verified against the script: window/document/localStorage/location/
// URLSearchParams + El{appendChild,contains,classList,style,dataset,addEventListener,value}).
const char* kShim = R"js(
function El(tag) {
  this.tagName = tag; this.children = []; this.style = {}; this.dataset = {}; this.value = '';
  this.parentElement = null; this._ev = {}; this._cls = {};
  var self = this;
  this.classList = {
    toggle: function(c, f) { if (f === undefined) { self._cls[c] = !self._cls[c]; } else { self._cls[c] = !!f; } },
    add: function(c) { self._cls[c] = true; }, remove: function(c) { delete self._cls[c]; },
    contains: function(c) { return !!self._cls[c]; }
  };
  this.appendChild = function(c) {
    var i = self.children.indexOf(c); if (i >= 0) self.children.splice(i, 1);
    self.children.push(c); c.parentElement = self; return c;
  };
  this.contains = function(c) { return self.children.indexOf(c) >= 0; };
  this.addEventListener = function(t, f) { (self._ev[t] = self._ev[t] || []).push(f); };
}
var __L = {}, __els = {}, __side = [];
var window = { innerWidth: 1280, addEventListener: function(t, f) { (__L[t] = __L[t] || []).push(f); } };
var document = {
  body: null,
  getElementById: function(id) { return __els[id] || null; },
  querySelectorAll: function(sel) {
    if (sel === '#pinned-cards > li, #saved-cards > li')
      return __els['pinned-cards'].children.concat(__els['saved-cards'].children);
    if (sel === '[data-grid-plugin]') return __side.slice();
    return [];
  }
};
var location = { search: '', pathname: '/', origin: 'http://test' };
// URL + history shim: grid sync() builds a shareable URL from location and replaceState's it
function URL(base) {
  var self = this, p = {};
  self.origin = (base && base.origin) || 'http://test'; self.pathname = (base && base.pathname) || '/';
  function parse(s) { var o = {}; String(s || '').replace(/^\?/, '').split('&').filter(Boolean).forEach(function(kv) { var a = kv.split('='); o[a[0]] = a[1] || ''; }); return o; }
  p = parse(base && base.search);
  Object.defineProperty(self, 'search', { get: function() { return self.searchParams.toString() ? '?' + self.searchParams.toString() : ''; }, set: function(v) { p = parse(v); } });
  self.searchParams = {
    set: function(k, v) { p[k] = String(v); },
    toString: function() { return Object.keys(p).map(function(k) { return k + '=' + p[k]; }).join('&'); }
  };
}
URL.prototype.toString = function() { return this.origin + this.pathname + this.search; };
var __history = { url: '' };
var history = { replaceState: function(_s, _t, u) { __history.url = String(u); } };
var localStorage = {
  _s: {},
  getItem: function(k) { return this._s[k] !== undefined ? this._s[k] : null; },
  setItem: function(k, v) { this._s[k] = String(v); }
};
function URLSearchParams(s) {
  this.m = {};
  var q = String(s || '').replace(/^\?/, '');
  if (q) q.split('&').forEach(function(p) { var kv = p.split('='); this.m[kv[0]] = kv[1] || ''; }, this);
}
URLSearchParams.prototype.get = function(k) { return k in this.m ? this.m[k] : null; };
URLSearchParams.prototype.has = function(k) { return k in this.m; };
// world(cards, prefs): fresh DOM; cards = [{name, plugin, dup, added, ... pinned:'1' => pin section}]
function __world(cards, prefs) {
  __els = { 'saved-cards': new El('ul'), 'pinned-cards': new El('ul'), 'lib-q': new El('input') };
  __side = []; document.body = new El('body'); __L = {};
  localStorage._s = {};
  if (prefs) localStorage.setItem('grid_prefs', JSON.stringify(prefs));
  cards.forEach(function(c) {
    var e = new El('li');
    for (var k in c) e.dataset[k] = String(c[k]);
    (c.pinned !== undefined ? __els['pinned-cards'] : __els['saved-cards']).appendChild(e);
  });
  return gridPage();
}
function __expect(cond, msg) { if (!cond) throw new Error(msg); }
)js";

class Qjs {
  public:
    Qjs() {
        rt = JS_NewRuntime();
        ctx = JS_NewContext(rt);
    }
    ~Qjs() {
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
    }
    std::string eval(const std::string& code, const char* name = "<gridjs>") {
        JSValue r = JS_Eval(ctx, code.c_str(), code.size(), name, JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(r)) {
            JSValue e = JS_GetException(ctx);
            std::string msg = "?";
            if (!JS_IsNull(e) && !JS_IsUndefined(e)) {
                size_t len = 0;
                const char* s = JS_ToCStringLen(ctx, &len, e);
                if (s != nullptr) msg.assign(s, len);
                JS_FreeCString(ctx, s);
                JSValue st = JS_GetPropertyStr(ctx, e, "stack");
                if (JS_IsString(st)) {
                    size_t sl = 0;
                    const char* ss = JS_ToCStringLen(ctx, &sl, st);
                    if (ss != nullptr) msg += " @ " + std::string(ss, sl);
                    JS_FreeCString(ctx, ss);
                }
                JS_FreeValue(ctx, st);
            }
            { // dump the failing script for post-mortem
                std::ofstream dbg("/tmp/opencode/gridjs_fail.js", std::ios::binary);
                dbg << code;
            }
            JS_FreeValue(ctx, e);
            JS_FreeValue(ctx, r);
            throw std::runtime_error("js: " + msg);
        }
        size_t len = 0;
        const char* s = JS_ToCStringLen(ctx, &len, r);
        std::string out;
        if (s != nullptr) out.assign(s, len);
        JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, r);
        return out;
    }
    JSRuntime* rt = nullptr;
    JSContext* ctx = nullptr;
};

// A card set with two real dup pairs and one single; added-at ties broken by name.
//  Zeta(dup0)  Beta(dup1)  Alpha(dup1)  Pin1(dup1,pinned)  Pin2(dup0,pinned)
const char* kCards = R"js([
  { name: 'Zeta', plugin: 'p1', dup: 0, added: 3 },
  { name: 'Beta', plugin: 'p2', dup: 1, added: 1 },
  { name: 'Alpha', plugin: 'p2', dup: 1, added: 2 },
  { name: 'Pin1', plugin: 'p3', dup: 1, added: 1, pinned: '1' },
  { name: 'Pin2', plugin: 'p3', dup: 0, added: 2, pinned: '1' }
])js";

} // namespace

TEST_CASE("grid component: dup chip re-engages cluster sort (library-dedup 3.2)") {
    Qjs js;
    js.eval(kShim, "shim");
    js.eval(extract_script(slurp(CPPISEKAI_GRID_SRC)), "gridcomp");

    SUBCASE("chip on: sort=dup, dup pairs adjacent, filtered single hidden") {
        auto out = js.eval(std::string{"(function(){\n"
                                      "  const c = __world("} +
                           kCards + R"js();
  c.init();
  __expect(c.dup === false && c.sort === 'read', 'initial defaults');
  c.toggleDup();
  __expect(c.dup === true && c.sort === 'dup', 'chip engages dup sort, got ' + c.sort);
  const names = __els['saved-cards'].children.map(x => x.dataset.name);
  __expect(JSON.stringify(names) === JSON.stringify(['Alpha', 'Beta', 'Zeta']), 'dup order adjacent, got ' + names);
  const zeta = __els['saved-cards'].children.find(x => x.dataset.name === 'Zeta');
  __expect(zeta.style.display === 'none', 'single filtered out, got ' + zeta.style.display);
  __expect(__els['pinned-cards'].children.filter(x => x.style.display === '').map(x => x.dataset.name).join() === 'Pin1', 'pin section intact');
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "chip-on: ", out);
    }

    SUBCASE("chip off: falls back to recent-read desc") {
        auto out = js.eval(std::string{"(function(){\n"
                                      "  const c = __world("} +
                           kCards + R"js();
  c.init(); c.toggleDup(); c.toggleDup();
  __expect(c.dup === false && c.sort === 'read', 'chip off restores read');
  __expect(c.sortDir.read === 'desc', 'read desc restored');
  const names = __els['saved-cards'].children.map(x => x.dataset.name);
  __expect(JSON.stringify(names) === JSON.stringify(['Zeta', 'Alpha', 'Beta']), 'read order, got ' + names);
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "chip-off: ", out);
    }

    SUBCASE("restored chip keeps cluster order (init engage)") {
        auto out = js.eval(std::string{"(function(){\n"
                                      "  const c = __world("} +
                           kCards + R"js(, { v: 2, dup: true, sort: 'read' });
  c.init();
  __expect(c.dup === true && c.sort === 'dup', 'init engages dup for restored chip, got ' + c.sort);
  const names = __els['saved-cards'].children.map(x => x.dataset.name);
  __expect(JSON.stringify(names) === JSON.stringify(['Alpha', 'Beta', 'Zeta']), 'cluster order on load, got ' + names);
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "restore: ", out);
    }

    SUBCASE("apply orders the DOM by the active sort (sort-driven append)") {
        auto out = js.eval(R"js(
(function(){
  const c = __world([{name:'b',plugin:'p',added:1},{name:'a',plugin:'p',added:2},{name:'c',plugin:'p',added:3}]);
  c.init();
  let names = __els['saved-cards'].children.map(x => x.dataset.name);
  __expect(JSON.stringify(names) === JSON.stringify(['c','a','b']), 'added-desc default, got ' + names);
  c.setSort('title');
  names = __els['saved-cards'].children.map(x => x.dataset.name);
  __expect(JSON.stringify(names) === JSON.stringify(['a','b','c']), 'title sort reorders DOM, got ' + names);
  for (let i = 1; i < names.length; i++) __expect(names[i-1].localeCompare(names[i]) <= 0, 'nondecreasing');
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "dom-order: ", out);
    }

    SUBCASE("filtered pinned card hides in place, pin section keeps order") {
        auto out = js.eval(std::string{"(function(){\n"
                                      "  const c = __world("} +
                           kCards + R"js();
  c.init(); c.toggleDup(); // dup on: Pin2 (dup0) filtered
  const pin = __els['pinned-cards'].children;
  const pin2 = pin.find(x => x.dataset.name === 'Pin2');
  __expect(pin.includes(pin2), 'Pin2 stays in pin section');
  __expect(pin2.style.display === 'none', 'filtered pin hidden in place');
  __expect(pin.find(x => x.dataset.name === 'Pin1').style.display === '', 'dup pin visible');
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "pin-filter: ", out);
    }

    SUBCASE("prefs persist through save()") {        auto out = js.eval(std::string{"(function(){\n"
                                      "  const c = __world("} +
                           kCards + R"js();
  c.init(); c.toggleDup();
  const p = JSON.parse(localStorage.getItem('grid_prefs'));
  __expect(p.v === 2 && p.dup === true && p.sort === 'dup', 'prefs saved, got ' + JSON.stringify(p));
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "prefs: ", out);
    }
}

TEST_CASE("grid component: state mirrors into a shareable URL (reader parity)") {
    Qjs js;
    js.eval(kShim, "shim");
    js.eval(extract_script(slurp(CPPISEKAI_GRID_SRC)), "gridcomp");

    SUBCASE("apply() writes the visible state to the URL") {
        auto out = js.eval(std::string{"(function(){\n  const c = __world("} + kCards + R"js();
  c.init();
  __expect(__history.url === 'http://test/', 'defaults leave a bare URL, got ' + __history.url);
  c.toggleDup();
  __expect(__history.url === 'http://test/?sort=dup&dir=asc&dup=1', 'dup state in URL, got ' + __history.url);
  c.q = 'zeta'; c.apply();
  __expect(__history.url.indexOf('q=zeta') > 0, 'query in URL, got ' + __history.url);
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "url-write: ", out);
    }

    SUBCASE("a shared URL restores state over saved prefs") {
        auto out = js.eval(std::string{"(function(){\n"
                                      "  location.search = '?q=zet&sort=title&dir=desc&f=done:show';\n"
                                      "  const c = __world("} +
                           kCards + R"js(, { v: 2, q: 'saved', sort: 'read' });
  c.init();
  __expect(c.q === 'zet', 'q from URL beats prefs, got ' + c.q);
  __expect(c.sort === 'title' && c.sortDir.title === 'desc', 'sort+dir from URL, got ' + c.sort + '/' + c.sortDir.title);
  __expect(c.filterState.done === 'show', 'filter chip from URL, got ' + c.filterState.done);
  location.search = '';
  return 'ok';})()
)js");
        CHECK_MESSAGE(out == "ok", "url-restore: ", out);
    }
}
