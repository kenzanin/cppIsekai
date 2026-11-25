// canonical status; unknown passes through, empty -> "unknown"
function normalize_status(s) {
  s = String(s || "").toLowerCase().replace(/[^a-z0-9]/g, "");
  if (s === "") return "unknown";
  if (s === "ongoing" || s === "ongoin" || s === "releasing" || s === "publishing") return "Ongoing";
  if (s === "completed" || s === "finished") return "Completed";
  if (s === "hiatus" || s === "onhaiatus") return "Hiatus";
  if (s === "dropped" || s === "cancelled" || s === "canceled") return "Dropped";
  if (s === "upcoming" || s === "notyetreleased") return "Upcoming";
  return s;
}

// get_body parity: "" on any failure (transport, non-200, malformed reply).
function get_body(url, headers) {
  const body = host.http.fetch_body(url, headers ? { headers: headers } : undefined);
  return typeof body === "string" ? body : "";
}

// host.re2.find returns [full, g1, ...] or []; safe group read.
function grp(m, i) {
  return m && m.length > i ? m[i] : "";
}

function decode_id(p) {
  if (typeof p === "string") return p;
  const v = p && p.id;
  return typeof v === "string" ? v : "";
}

const MONTHS = { Jan: 0, Feb: 1, Mar: 2, Apr: 3, May: 4, Jun: 5, Jul: 6, Aug: 7, Sep: 8, Oct: 9, Nov: 10, Dec: 11 };
// "Jul 22,2014" (mobile chapter <span class=time>) -> epoch seconds; 0 if absent
function ts_of(s) {
  const m = /^([A-Z][a-z]{2}) (\d{1,2}),(\d{4})$/.exec((s || "").trim());
  if (!m) return 0;
  const t = Date.UTC(+m[3], MONTHS[m[1]], +m[2]) / 1000;
  return isFinite(t) ? t : 0;
}

// Same detail page. Links: /manga/{slug}/v09/c011.5/ or /manga/{slug}/c070/.
// Decimal-safe: c([0-9][0-9.]*)$ anchored at path end (c011.5 AND c012.1).
// Ascending order = cppIsekai reading-order convention (host re-sorts too).
module.exports = { normalize_status, get_body, grp, decode_id, ts_of };
