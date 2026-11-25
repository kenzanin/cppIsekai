const { get_body, rsc_chunks, find_object, array_after, BASE } = require("common");
function get_genres() {
  const html = get_body(BASE + "/en/manga");
  const genres = [];
  if (html === "") return { ok: true, data: genres };
  const seen = {};
  const skip = {
    "latest update": 1, "a-z": 1, "most chapters": 1, "rating": 1, "all": 1,
    "ongoing": 1, "completed": 1, "hiatus": 1,
  };
  for (const m of host.re2.find_all(
    "<button[^>]*>([A-Za-z][A-Za-z &-]{1,18})</button>",
    html
  )) {
    const name = host.text.trim(m[1]);
    if (!name || skip[name] || seen[name]) continue;
    seen[name] = 1;
    genres.push(name);
  }
  return { ok: true, data: genres };
}

// Host seam: envelope dispatch, parity with the lua plugins' handle().
module.exports = { get_genres };
