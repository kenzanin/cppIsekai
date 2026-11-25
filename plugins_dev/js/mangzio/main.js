// Mangzio JS plugin -- per-action modules.
const { search_manga } = require("search");
const { get_manga_detail } = require("detail");
const { get_chapter_list } = require("chapters");
const { get_page_list } = require("pages");
const { get_genres } = require("genres");

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
      return get_genres();
    default:
      return { ok: false, error: "unknown method " + ((req && req.method) || "") };
  }
}
