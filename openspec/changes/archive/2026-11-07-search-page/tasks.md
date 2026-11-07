# Tasks: search-page

## 1. Mangadex plugin
- [x] 1.1 `search`: page param (1-based, clamp), tags AND-filter (name->uuid map from /manga/tag), response `{items,total}`, keep followedCount desc
- [x] 1.2 `genres` method from /manga/tag (name.en list)

## 2. Page
- [x] 2.1 `templates/search.mustache` + route: picker (enabled only) + query + button; genre chips row (hide on failure); results count; cover grid w/ hue fallback; pagination x2; skeleton/error/empty states; pushState round-trip
- [x] 2.2 ABI search response consumer updates (page expects data.items/total)

## 3. Verification
- [x] 3.1 luac; live: one piece -> total>0 page1/2 disjoint; tags AND narrows; genres lists Isekai/Fantasy
- [x] 3.2 UI: empty state, error+retry (offline plugin), URL reload restores view, disabled plugin notice
