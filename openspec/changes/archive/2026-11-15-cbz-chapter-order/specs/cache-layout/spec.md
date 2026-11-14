# Spec Delta: cache-layout

## MODIFIED Requirements

### Requirement: CBZ export route
`GET /export/cbz?plugin=<id>&slug=<slug>` SHALL stream a CBZ (zip) of the cached chapter
page images for that manga, entries named `chNNN/pNNN.<ext>` after the zero-padded file
paths. Archive order SHALL be reading order: entries SHALL be ordered by chapter number and
then by page number, compared **numerically**. The implementation SHALL NOT rely on
lexicographic ordering of the entry names, because chapter directories are padded to a
minimum of three digits and a four-digit chapter name (`ch1000`) sorts before `ch999`.
An optional `/<chNNN>` suffix on the slug SHALL narrow the export to directories whose name
starts with that prefix. Empty cache for a requested slug SHALL produce an empty archive,
not an error.

#### Scenario: Downloadable CBZ
- **WHEN** a slug whose pages were cached is exported
- **THEN** the response is a valid zip whose entries are the cached page images, openable by
  any reader

#### Scenario: Unknown slug still 200
- **WHEN** `GET /export/cbz` targets a slug with no cached pages
- **THEN** the response is an empty-but-valid zip, not a 404

#### Scenario: Chapter count crosses the padding width
- **WHEN** a slug has cached pages under `ch999` and `ch1000` and is exported
- **THEN** the entry for chapter 999 precedes the entry for chapter 1000 in the archive,
  matching reading order despite `ch1000` sorting first lexicographically

#### Scenario: Chapter filter suffix
- **WHEN** the slug carries a `/ch003` suffix
- **THEN** only chapter directories whose name starts with `ch003` are exported
