# References
* [cppIsekai dependency API surface](dependency-api.md) - Per-dependency functions and entry points with include paths; header-verified, includes curl_easy_impersonate, SimpleIni GetLongValue and sqlite_orm escape/limit/pragma caveats
* [Source layer map and rules (store / log / http)](code-layers.md) - Layer map of the shipped tree: Store is the only SQL, QueueSink is the whole log bridge, on_db() is the only cross-thread path; rules for future modules
