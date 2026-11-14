# Tasks: cbz-chapter-order

## 1. Regression test first

- [ ] 1.1 Add a `tests/test_export.cpp` case that caches `ch999/p001.webp` and
      `ch1000/p001.webp` and asserts the `ch999` entry precedes `ch1000` in the returned zip
- [ ] 1.2 Run it and confirm it fails against the current `std::map` implementation, recording
      the two offsets in the task notes

## 2. Fix the ordering

- [ ] 2.1 Replace the `std::map<std::string, fs::path>` accumulation in `cbz::build` with a
      sequence of `{chapter, page, entry name, path}` sorted by numeric chapter then numeric page
- [ ] 2.2 Parse chapter from the `ch<digits>` directory name and page from the `p<digits>.<ext>`
      file name; unparsable names sort last, deterministically, and never abort the export
- [ ] 2.3 Keep the emitted entry names byte-identical (`chNNN/pNNN.<ext>`) and keep the
      `/chNNN` slug-suffix filter working on the directory-name prefix

## 3. Verification

- [ ] 3.1 `xmake -y` clean with `-Wall -Wextra` (no new warnings)
- [ ] 3.2 `xmake test` green, including the existing `tests/test_export.cpp` cases
- [ ] 3.3 Export a cached slug by hand and confirm entry order matches reading order

## 4. Spec

- [ ] 4.1 Confirm the modified `cache-layout` requirement matches the shipped behaviour
      (numeric ordering, no cover entry, chapter-suffix filter)
