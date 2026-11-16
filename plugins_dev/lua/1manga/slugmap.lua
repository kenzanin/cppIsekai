-- Name -> site slug map filled by the genres fetch (the UI only posts genre
-- names back). require() is cached per Lua state, so this table is the one
-- shared across genres() and search() calls inside that state's lifetime.
return { genre_slug = {} }
