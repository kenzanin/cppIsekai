# Findings
* [Host image fetch is plugin-blind: no Referer (js mangatown pilot finding)](js-image-referer-gap.md) - Image CDNs that require Referer 403 through the host cache/reader path; fix = per-plugin image-referer plumbed to the img fetcher.
