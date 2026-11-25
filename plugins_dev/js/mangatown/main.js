// MangaTown (m.mangatown.com) JS pilot port -- per-action modules.
const { search_manga } = require("search");
const { get_manga_detail } = require("detail");
const { get_chapter_list } = require("chapters");
const { get_page_list } = require("pages");

// Host seam: envelope dispatch, parity with the lua plugins' handle().
function handle(req) {
  const p = (req && req.params) || {};
  switch (req && req.method) {
    case "search":
      return search_manga(p);
    case "detail":
      return get_manga_detail(p);
    case "chapters":
      return get_chapter_list(p);
    case "pages":
      return get_page_list(p);
    case "genres":
      return { ok: true, data: [
        "4 Koma", "Action", "Adventure", "Comedy", "Cooking", "Doujinshi", "Drama", "Ecchi", "Fantasy", "Gender Bender", "Harem", "Historical", "Horror", "Martial Arts", "Mature", "Mecha", "Music", "Mystery", "One Shot", "Psychological", "Reverse Harem", "Romance", "School Life", "Sci Fi", "Shotacon", "Slice Of Life", "Smut", "Sports", "Supernatural", "Suspense", "Tragedy", "Vampire", "Webtoons", "Youkai",
      ] };
      return get_page_list(p);
    default:
      return { ok: false, error: "unknown method " + ((req && req.method) || "") };
  }
}
