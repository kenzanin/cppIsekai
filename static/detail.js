function detailPage() {
  const parts = location.pathname.split('/').filter(Boolean); // view manga <plugin> <slug...>
  return {
    plugin: parts[2] || '', slug: parts.slice(3).join('/') || '',
    detail: {}, chapters: [], cached: false, loading: true, refreshing: false, pe: window.CFG.pe,
    err: '', coverBroken: false, hint: null, related: [], pluginName: '', pluginLogoOk: true, pluginCaps: '',
    inLibrary: false, libraryBusy: false, pinned: false,
    enrichment: null, enrichLastFetched: 0, enrichSources: [],
    enrichOpen: false, enrichKind: 'detail', enrichBusy: false, busySources: {}, cand: {}, enrichErr: '',
    hiddenTags: [],
    chPage: 0, showHidden: false, readState: {}, cbzReady: {}, // id -> {skip, read} (chapter-list-controls 1.2); cbzReady: chapter fully cached
    get pageCount() { return Math.max(1, Math.ceil(this.visibleChapters().length / 20)); },
    // [1 2 3 ... p-1 p p+1 ... max-2 max-1 max] window; 0 marks the ellipsis gap
    chWindow() {
      const p = this.chPage + 1, max = this.pageCount
      const nums = [...new Set([1, 2, 3, p - 1, p, p + 1, max - 2, max - 1, max])]
        .filter(n => n >= 1 && n <= max).sort((a, b) => a - b)
      const out = []
      let prev = 0
      for (const n of nums) { if (n - prev > 1) out.push(0); out.push(n); prev = n }
      return out
    },
    visibleChapters() { return this.showHidden ? this.chapters : this.chapters.filter(c => !c.done); },
    chPageRows() { return this.visibleChapters().slice(this.chPage * 20, this.chPage * 20 + 20); },
    hiddenCount() { return this.chapters.length - this.visibleChapters().length; },
    readMax(ch) { return (this.readState[ch.id] && this.readState[ch.id].read) || 0; },
    chStats(ch) { const s = this.readState[ch.id] || {}; return ((typeof s.total === 'number') ? s.total : '-') + '/' + ((typeof s.cached === 'number') ? s.cached : '-') + '/' + this.readMax(ch); },
    totalOf(ch) { const t = this.readState[ch.id] && this.readState[ch.id].total; return (typeof t === 'number' && t > 0) ? t : 0; },
    isRead(ch) { return !!(this.readState[ch.id] && this.readState[ch.id].done); }, // chapter-state: bit0 is the single truth
    async loadChapterState() {
      if (!this.chapters.length) return;
      try {
        const r = await fetch('/chapter/state', {method: 'POST', headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({plugin: this.plugin, slug: this.slug, ids: this.chapters.map(c => c.id)})});
        if (r.ok) { const j = await r.json(); this.chapters.forEach(c => { if (j[c.id]) c.done = !!j[c.id].skip;
          const t = j[c.id] && j[c.id].total, cc = j[c.id] && j[c.id].cached;
          if (t > 0 && cc >= t) this.cbzReady[c.id] = true; // fully cached -> cbz button on
        }); this.readState = j; }
      } catch (e) { /* list renders without state */ }
    },
    async toggleRead(ch) {
      this.err = '';
      const on = !this.isRead(ch);
      this.readState[ch.id] = {...(this.readState[ch.id] || {}), done: on || undefined};
      try { const r = await fetch('/chapter/flags', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({plugin: this.plugin, slug: this.slug, ids: [ch.id], ...(on ? {set: 1} : {clear: 1})})});
        if (r.ok) window.toast('success', on ? 'Marked read' : 'Marked unread'); else window.toast('error', 'update failed'); } catch (e) { window.toast('error', String(e)); }
    },
    async markAllRead(on) {
      this.err = '';
      if (!this.chapters.length) return;
      if (!confirm(on ? `Mark all ${this.chapters.length} chapters read?` : 'Clear read status on all chapters?')) return;
      this.chapters.forEach(c => { this.readState[c.id] = {...(this.readState[c.id] || {}), done: on || undefined}; });
      try { const r = await fetch('/chapter/flags', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({plugin: this.plugin, slug: this.slug, all: true, ...(on ? {set: 1} : {clear: 1})})});
        if (r.ok) window.toast('success', on ? `Marked all ${this.chapters.length} read` : 'Cleared read status'); else window.toast('error', 'update failed'); } catch (e) { window.toast('error', String(e)); }
    },
    async markPrevRead(ch) { // chapter-state 4.2: canonical (number-asc) order, number <= picked
      this.err = '';
      const canon = [...this.chapters].sort((a, b) => (a.number || 0) - (b.number || 0));
      const ids = canon.filter(c => (c.number || 0) <= (ch.number || 0)).map(c => c.id);
      if (ids.length < 2 || !confirm(`Mark ${ids.length} chapters (up to ${this.chLabel(ch)}) read?`)) return;
      ids.forEach(id => { this.readState[id] = {...(this.readState[id] || {}), done: true}; });
      try { const r = await fetch('/chapter/flags', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({plugin: this.plugin, slug: this.slug, ids, set: 1})});
        if (r.ok) window.toast('success', `Marked ${ids.length} read`); else window.toast('error', 'update failed'); } catch (e) { window.toast('error', String(e)); }
    },
    async toggleDone(ch) {
      this.err = '';
      ch.done = !ch.done;
      try { const r = await fetch(`/chapter/${encodeURIComponent(ch.id)}/skip`, {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({skipped: ch.done, plugin: this.plugin, slug: this.slug})});
        if (r.ok) window.toast('success', ch.done ? 'Chapter skipped' : 'Chapter unskipped'); else window.toast('error', 'skip failed'); } catch (e) { window.toast('error', String(e)); }
    },
    async boot() {
      await this.loadPluginMeta(); // caps gate related fetch inside load()
      this.loadLibrary();
      this.loadEnrichSources();
      await this.load();
      // cover img renders via x-if after load(); paintDim at DOMContentLoaded missed it
      this.$nextTick(() => window.paintDim && window.paintDim());
    },
    normAltTitle(s) { return String(s || '').toLowerCase().replace(/[^\p{L}\p{N}]+/gu, ' ').trim(); },
    altTitleCandidates() {
      // merge main title + altTitles from all sources; normalized dedupe; MangaDex wins on collision
      const seen = new Map();
      for (const src of this.enrichSources) {
        const c = this.candFor(src.id, 'detail');
        const alts = c && c.data && Array.isArray(c.data.alt_titles) ? c.data.alt_titles : [];
        for (const t of alts) {
          const raw = String(t && t.title || '').trim();
          if (!raw) continue;
          const key = this.normAltTitle(raw);
          if (!key) continue;
          if (!seen.has(key)) {
            seen.set(key, { title: raw, lang: String(t.lang || ''), main: !!t.main, source: src.id });
          }
        }
      }
      const arr = [...seen.values()];
      arr.sort((a, b) => (b.main ? 1 : 0) - (a.main ? 1 : 0)); // main titles first
      return arr;
    },
    relKey(it) { return String((it && (it.url || it.id || it.name)) || '').toLowerCase(); },
    mergedRelated() { // wizard list: MD first, MU only on non-collision; normalized key url->id->name
      const out = []; const seen = new Set();
      const push = (it, source) => {
        if (!it) return;
        const name = String(it.name || '').trim(); const url = String(it.url || '').trim(); const id = String(it.id || '').trim();
        if (!name || !url) return;
        const k = this.relKey({ url, id, name });
        if (seen.has(k)) return;
        seen.add(k);
        out.push({ name, url, id: id || undefined, source });
      };
      for (const src of ['mangadex-enrichment', 'mangaupdates-enrichment']) {
        const c = this.candFor(src, 'recommendations');
        const items = (c && c.ok && c.data && Array.isArray(c.data.items)) ? c.data.items : [];
        for (const it of items) push(it, src);
      }
      return out;
    },
    async addAllRelated() {
      const merged = this.mergedRelated();
      const chosen = { ...this.chosenNow() };
      if (merged.length) chosen.related_add = merged.map(({ name, url, id }) => id ? { name, url, id } : { name, url });
      else delete chosen.related_add;
      const r = await fetch('/api/enrichment/chosen', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ plugin: this.plugin, slug: this.slug, chosen })});
      if (!r.ok) { this.enrichErr = 'save failed'; window.toast('error', 'save failed'); return; }
      await this.load();
      window.toast('success', 'Related added');
    },
    async removeAllRelated() {
      const chosen = { ...this.chosenNow() };
      delete chosen.related_add;
      const r = await fetch('/api/enrichment/chosen', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ plugin: this.plugin, slug: this.slug, chosen })});
      if (!r.ok) { this.enrichErr = 'save failed'; window.toast('error', 'save failed'); return; }
      await this.load();
      window.toast('success', 'Related removed');
    },
    srcData(id) { const c = this.candFor(id, 'detail'); return (c && c.data) || {}; },
    mergedList(field) { // MD first; MU shown only when no normalized collision
      const md = this.srcData('mangadex-enrichment')[field];
      const mu = this.srcData('mangaupdates-enrichment')[field];
      const out = []; const seen = new Set();
      const push = (v, source) => { const raw = String(v || '').trim(); const k = this.normAltTitle(raw); if (k && !seen.has(k)) { seen.add(k); out.push({ name: raw, source }); } };
      (Array.isArray(md) ? md : []).forEach(v => push(v, 'mangadex-enrichment'));
      (Array.isArray(mu) ? mu : []).forEach(v => push(v, 'mangaupdates-enrichment'));
      return out;
    },
    mergedSynopses() {
      const md = this.srcData('mangadex-enrichment').synopsis;
      const mu = this.srcData('mangaupdates-enrichment').synopsis;
      const out = [];
      if (md) out.push({ src: 'mangadex-enrichment', text: md, key: this.normAltTitle(md) });
      if (mu) { const k = this.normAltTitle(mu); if (!out.length || out[0].key !== k) out.push({ src: 'mangaupdates-enrichment', text: mu, key: k }); }
      return out;
    },
    async loadEnrichSources() { // loaded enrichment plugins (picker needs the list)
      try {
        const j = await (await fetch('/api/enrichment')).json();
        this.enrichSources = (j && j.sources) || [];
      } catch (e) { this.enrichSources = []; }
    },
    async openEnrich(kind, force) { // per-source fetch; cached unless force=true
      this.enrichKind = kind; this.enrichOpen = true; this.enrichErr = ''; this.enrichBusy = true;
      let failMsg = '';
      for (const src of this.enrichSources) {
        this.busySources = { ...this.busySources, [src.id]: true };
        try {
          const r = await fetch(`/api/enrichment/${encodeURIComponent(src.id)}/${kind}?plugin=${encodeURIComponent(this.plugin)}&slug=${encodeURIComponent(this.slug)}&force=${force ? 1 : 0}`);
          const j = await r.json();
          if (j && j.ok === false) failMsg = j.error || 'enrichment failed';
          this.cand = { ...this.cand, [src.id + ':' + kind]: j };
          if (j && j.fetched_at) this.enrichLastFetched = Math.max(this.enrichLastFetched || 0, j.fetched_at);
          if (kind === 'detail') { // wizard also needs recommendations for the related-readings segment
            try {
              const rr = await fetch(`/api/enrichment/${encodeURIComponent(src.id)}/recommendations?plugin=${encodeURIComponent(this.plugin)}&slug=${encodeURIComponent(this.slug)}&force=${force ? 1 : 0}`);
              this.cand = { ...this.cand, [src.id + ':recommendations']: await rr.json() };
            } catch (e) { this.cand = { ...this.cand, [src.id + ':recommendations']: { ok: false, error: String(e) } }; }
          }
        } catch (e) { this.cand = { ...this.cand, [src.id + ':' + kind]: { ok: false, error: String(e) } }; failMsg = String(e); }
        this.busySources = { ...this.busySources, [src.id]: false };
      }
      this.enrichBusy = false;
      if (failMsg) window.toast('error', failMsg); else window.toast('success', 'Enrichment fetched');
    },
    candFor(id, kind) { return this.cand[id + ':' + (kind || this.enrichKind)] || null; },
    chosenNow() { return (this.enrichment && this.enrichment.chosen) || {}; },
    async saveChosen(patch) { // full-overwrite POST; server re-merges on next load
      const chosen = { ...this.chosenNow(), ...patch };
      for (const k of Object.keys(chosen)) if (chosen[k] === undefined || chosen[k] === null) delete chosen[k];
      const r = await fetch('/api/enrichment/chosen', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ plugin: this.plugin, slug: this.slug, chosen })});
      if (!r.ok) { this.enrichErr = 'save failed'; window.toast('error', 'save failed'); return; }
      await this.load(); // re-renders merged detail + enrichment block
      window.toast('success', 'Saved');
    },
    async pickField(field, value) { await this.saveChosen({ [field]: value }); },
    async toggleName(field, name) { // artist/author: union of chosen names (array)
      const cur = new Set(this.chosenNow()[field] || []);
      if (cur.has(name)) cur.delete(name); else cur.add(name);
      const arr = [...cur];
      await this.saveChosen({ [field]: arr.length ? arr : undefined });
    },
    async toggleTag(tag) { // tags_add: add/remove one tag
      const cur = new Set(this.chosenNow().tags_add || []);
      if (cur.has(tag)) cur.delete(tag); else cur.add(tag);
      const arr = [...cur];
      await this.saveChosen({ tags_add: arr.length ? arr : undefined });
    },
    async toggleHideTag(tag) { // tags_hidden: hide a rendered tag without deleting it
      const cur = new Set((this.enrichment && this.enrichment.chosen && this.enrichment.chosen.tags_hidden) || []);
      const key = String(tag).toLowerCase();
      const plain = new Set([...cur].map(x => String(x).toLowerCase()));
      if (plain.has(key)) { for (const x of [...cur]) if (String(x).toLowerCase() === key) cur.delete(x); }
      else cur.add(tag);
      const arr = [...cur];
      await this.saveChosen({ tags_hidden: arr.length ? arr : undefined });
    },
    async resetCover() {
      try { const r = await fetch('/api/enrichment/reset', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ plugin: this.plugin, slug: this.slug, mode: 'cover' })});
        if (!r.ok) { window.toast('error', 'reset failed'); return; }
        await this.load(); window.toast('success', 'Cover reset'); } catch (e) { window.toast('error', String(e)); }
    },
    async resetDetail() { // clears non-tag choseD fields, keeps chosen.cover (task 4.3)
      try { const r = await fetch('/api/enrichment/reset', {method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ plugin: this.plugin, slug: this.slug, mode: 'detail' })});
        if (!r.ok) { window.toast('error', 'reset failed'); return; }
        await this.load(); window.toast('success', 'Detail reset'); } catch (e) { window.toast('error', String(e)); }
    },
    fmtEnrichDate(ts) { return ts ? this.fmtDate(ts) : ''; },
    async load() {
      this.loading = true; this.err = '';
      this.loadRelated(); // fire-and-forget (D4): failure never blocks the page
      try {
        // continue-hint badge (reader-page 2.2): stored position, if any.
        if (this.plugin && this.slug) {
          const r = await fetch(`/api/position/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}`);
          if (r.ok) {
            const p = await r.json();
            if (p && p.ch) {
              this.hint = {
                text: `continue ch ${p.ch} p ${p.page || 1}`,
                url: `/read/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}?ch=${p.ch}`
              };
            }
          }
        }
        const d = await (await fetch(`/api/detail/${this.plugin}/${encodeURIComponent(this.slug)}`)).json();
        const c = await (await fetch(`/api/chapters/${this.plugin}/${encodeURIComponent(this.slug)}`)).json();
        if (!d.ok || !c.ok) { this.err = d.error || c.error || 'load failed'; return; }
        this.detail = d.data || {}; this.cached = !!(d.cached || c.cached);
        this.enrichment = (d.enrichment) || null;
        this.enrichLastFetched = Math.max(this.enrichLastFetched || 0, (this.enrichment && this.enrichment.fetched_at) || 0);
        this.hiddenTags = (this.enrichment && this.enrichment.chosen && this.enrichment.chosen.tags_hidden) || [];
        this.chapters = (Array.isArray(c.data) ? c.data : []).slice().sort((a, b) => (b.number ?? 0) - (a.number ?? 0)); // lua empty table marshals {} not []
        this.mergeSavedRelated(); // re-merge chosen.related_add now that /api/detail is in
        await this.loadChapterState(); // chapters are fresh objects -- re-apply done/read flags + cbz gating
      } catch (e) { this.err = String(e); }
      this.loading = false;
    },
    async refresh() { // ponytail: no LED/disable via runFeedback -- wrap body in runFeedback(job, btn) when the LED lands
      this.err = '';
      this.refreshing = true;
      try {
        const r = await (await fetch(`/api/refresh/${this.plugin}/${encodeURIComponent(this.slug)}`, {method: 'POST'})).json();
        if (!r.ok) { this.err = r.error || 'refresh failed'; window.toast('error', this.err); } else { await this.load(); this.cached = false; window.toast('success', 'Refreshed'); }
      } catch (e) { this.err = String(e); window.toast('error', this.err); }
      this.refreshing = false;
    },
    chNum(n) { const [i, f] = String(n).split('.'); return i.padStart(3, '0') + (f ? '.' + f : ''); },
    chLabel(ch) { const n = Number(ch.number); return Number.isFinite(n) ? 'ch ' + this.chNum(n) : (ch.title || 'ch'); },
    chSlug(ch) { const n = Number(ch.number); return Number.isFinite(n) ? 'ch' + this.chNum(n) : 'chx'; },
    // epoch seconds (mangafire) or an ISO date string (comix/4ucomics/thunderscan) -- both shapes
    // reach the templates; an ISO date through `ts*1000` was NaN and blanked the date column.
    fmtDate(ts) { if (!ts) return ''; const n = typeof ts === 'number' ? ts : (/^\d+$/.test(ts) ? Number(ts) : null); const d = n === null ? new Date(ts) : new Date(n * 1000); return isNaN(d) ? '' : d.toISOString().slice(0, 10); },
    scraperRelated: [], // base list from the scraper plugin; chosen.related_add is merged on top
    mergeSavedRelated() {
      const seen = new Set(); const merged = [];
      const push = (it) => { const k = this.relKey(it); if (k && !seen.has(k)) { seen.add(k); merged.push(it); } };
      for (const it of this.scraperRelated) push(it);
      const saved = this.enrichment && this.enrichment.chosen && this.enrichment.chosen.related_add;
      for (const it of (Array.isArray(saved) ? saved : [])) push(it);
      this.related = merged;
    },
    startOrContinue() { // [add to library] then [start / continue reading] -> reader at stored chapter
      if (!this.inLibrary) return;
      // chapters are sorted newest-first, so the first chapter read is the LAST row
      const first = this.chapters && this.chapters.length ? this.chapters[this.chapters.length - 1] : null;
      const target = this.hint && this.hint.url ? this.hint.url
        : `/read/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}?ch=${
          first ? Number(first.number) || first.number : '1'
        }`;
      location.href = target;
    },
    async loadRelated() { // scraper related only (cap-gated); enrichment recommendations render only via Add all (chosen.related_add)
      const grab = (url) => fetch(url).then(r => r.ok ? r.json() : null).catch(() => null);
      try {
        const wantsRelated = (this.pluginCaps || '').split(',').map(s => s.trim()).includes('related');
        const scraper = wantsRelated ? await grab(`/api/related/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}`) : null;
        this.scraperRelated = (scraper && scraper.ok && scraper.data && Array.isArray(scraper.data.items)) ? scraper.data.items : [];
        this.mergeSavedRelated();
      } catch (e) { /* related section stays hidden */ }
    },
    async toggleLibrary() {
      if (this.libraryBusy) return;
      this.err = '';
      this.libraryBusy = true;
      try {
        const url = this.inLibrary ? '/api/library/remove' : '/api/library';
        const body = {plugin: this.plugin, slug: this.slug, name: (this.detail && this.detail.name) || this.slug,
                      cover: (this.detail && this.detail.cover) || ''};
        const r = await fetch(url, {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body)});
        const j = await r.json().catch(() => null);
        if (!r.ok || !j || j.ok === false) { this.err = (j && j.error) || 'library failed'; window.toast('error', this.err); }
        else { this.inLibrary = !this.inLibrary; if (!this.inLibrary) this.pinned = false; window.toast('success', this.inLibrary ? 'Added to library' : 'Removed from library'); }
      } catch (e) { this.err = String(e); window.toast('error', this.err); }
      this.libraryBusy = false;
    },
    async togglePin() { // library-pin: membership in the grid's Pinned section
      if (this.libraryBusy || !this.inLibrary) return;
      this.err = '';
      this.libraryBusy = true;
      try {
        const r = await fetch('/api/library/pin', {method: 'POST', headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({plugin: this.plugin, slug: this.slug, pinned: !this.pinned})});
        const j = await r.json().catch(() => null);
        if (!r.ok || !j || j.ok === false) { this.err = (j && j.error) || 'pin failed'; window.toast('error', this.err); }
        else { this.pinned = !this.pinned; window.toast('success', this.pinned ? 'Pinned' : 'Unpinned'); }
      } catch (e) { this.err = String(e); window.toast('error', this.err); }
      this.libraryBusy = false;
    },
    async loadLibrary() {
      try {
        const r = await fetch('/api/library');
        if (r.ok) {
          const j = await r.json();
          const me = (j.data || []).find(m => m.plugin === this.plugin && m.slug === this.slug);
          this.inLibrary = !!me;
          this.pinned = !!(me && me.pinned);
        }
      } catch (e) { /* toggle stays at default false; err line untouched */ }
    },
    async loadPluginMeta() {
      try {
        const r = await fetch('/api/plugins');
        const j = r.ok ? await r.json() : {};
        const m = [...(j.plugins || []), ...(j.enrichment || [])].find(p => p.key === this.plugin);
        this.pluginCaps = (m && m.caps) || '';
        this.pluginName = (m && m.name) || this.plugin;
      } catch (e) { this.pluginName = this.plugin; }
    },
  };
}
