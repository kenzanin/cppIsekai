const { get_body, grp, decode_id, ts_of } = require("common");
function get_chapter_list(p) {
  const id = decode_id(p);
  const out = [];
  if (id !== "") {
    const html = get_body("https://m.mangatown.com/manga/" + id + "/");
    const seen = {};
    if (html !== "") {
      const pat =
        '(?s)href="/manga/' + host.re2.quote(id) + '/((?:v[0-9]+/)?c[0-9.]+)/".*?class="time">([^<]+)<';
      for (const m of host.re2.find_all(pat, html)) {
        const rest = m[1];
        const num = parseFloat(grp(host.re2.find("c([0-9][0-9.]*)$", rest), 1));
        if (isNaN(num) || seen[rest]) continue;
        seen[rest] = 1;
        out.push({
          id: id + "/" + rest,
          number: num,
          title: "Chapter " + num,
          release_at: ts_of(m[2]),
          pages: 0,
        });
      }
      out.sort((a, b) => a.number - b.number);
    }
  }
  return { ok: true, data: out };
}

// ─── pages ─────────────────────────────────────────────────────────────────
// Desktop chapterfun path: 2 images per request (a 34-page mobile chapter is
// N round-trips). Mobile fallback scrapes <img id="image"> per page.
module.exports = { get_chapter_list };
