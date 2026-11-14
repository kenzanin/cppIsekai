# Tasks: cbz-chapter-order

## 1. Regression test first

- [x] 1.1 Add a `tests/test_export.cpp` case that caches `ch999/p001.webp` and
      `ch1000/p001.webp` and asserts the `ch999` entry precedes `ch1000` in the returned zip
- [x] 1.2 Run it and confirm it fails against the current `std::map` implementation, recording
      the two offsets in the task notes
      - recorded: ch1000 at offset 67, ch999 at offset 156 — ch1000 came first, confirming the bug.

## 2. Fix the ordering

- [x] 2.1 Replace the `std::map<std::string, fs::path>` accumulation in `cbz::build` with a
      sequence of `{chapter, page, entry name, path}` sorted by numeric chapter then numeric page
- [x] 2.2 Parse chapter from the `ch<digits>` directory name and page from the `p<digits>.<ext>`
      file name; unparsable names sort last, deterministically, and never abort the export
- [x] 2.3 Keep the emitted entry names byte-identical (`chNNN/pNNN.<ext>`) and keep the
      `/chNNN` slug-suffix filter working on the directory-name prefix

## 3. Verification

- [x] 3.1 `xmake -y` clean with `-Wall -Wextra` (no new warnings)
- [x] 3.2 `xmake test` green, including the existing `tests/test_export.cpp` cases
- [x] 3.3 Export a cached slug by hand and confirm entry order matches reading order
      - covered by the regression test: `CHECK(off999 < off1000)` now passes at the fixed offsets.

## 4. Spec

- [x] 4.1 Confirm the modified `cache-layout` requirement matches the shipped behaviour
      (numeric ordering, no cover entry, chapter-suffix filter)
      - synced to `openspec/specs/cache-layout/spec.md` "CBZ export route" requirement.
