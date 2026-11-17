---
type: Decision
title: Drop date (HowardHinnant) dependency; host_text dates via stdlib
description: "Removed date 3.0.4 from xmake and host_text.hpp; format via gmtime_r+strftime, fixed-layout parse via strptime; suite 18/18 green."
tags: [xmake, dependencies, host-text, datetime, yagni]
generated: { by: agent/mcp, at: "2026-11-17T21:50:51Z" }
status: stable
---

Removed the last runtime use of the date (HowardHinnant) xrepo package. src/plugin/host_text.hpp: detail::format_date = system_clock::to_time_t + gmtime_r + strftime(%Y-%m-%d); date_to_iso fixed-layout branch = strptime loop over kLayouts + strftime; includes <ctime>, dropped <date/date.h> and <sstream>. xmake.lua: date 3.0.4 gone from add_requires and all target reqs. -Wall -Wextra clean; full suite 18/18. Rationale: two format/parse helpers never needed a header library; one less xrepo dep to fetch/audit. Caveat: strptime is a GNU extension, fine on the glibc targets this daemon ships for; revisit only if a non-glibc target appears.

# Related Concepts
- [Detail page dates: plugins emit both epoch seconds and ISO strings](../findings/plugin-date-shapes-2026-11-18.md): Host date conversion is stdlib-only; the same epoch-vs-ISO split shows up at the template boundary.
