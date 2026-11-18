function readerPage() {
  // /read/<plugin>/<slug..>?ch=&page=&mode=&dir= -- 3.2's resume seeds
  // page/mode/dir; without them: first page, paged rtl (long-strip default).
  const parts = location.pathname.split('/').filter(Boolean);
  return {
    plugin: decodeURIComponent(parts[1] || ''),
    slug: decodeURIComponent(parts.slice(2).join('/')) || '',
    ch: 1, chId: '', pages: [], prev_ch: 0, next_ch: 0, prev_ch_id: '', next_ch_id: '',
    page: 1, mode: 'paged', dir: 'ltr', zoom: 'fit-w', zoomPct: 100, wheelAt: 0,
    navShown: false, done: false,
    seen: [], err: '', io: null, chaptersList: [],
    _prefetch: new Map(), _reqToken: 0, _chaptersReady: null, t: null,
    spacer: 'data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7',
    async boot() {
      const q = new URLSearchParams(location.search);
      // seed: stored position (window.RESUME: ch/page/mode/dir),
      // then ?ch and ?page override, then JS defaults (ch 1, paged rtl).
      const resumeCh = Number(RESUME.ch);
      this.ch = Number.isFinite(resumeCh) ? Math.max(0, resumeCh) : 1;
      this.page = Number(RESUME.page) || 1;
      // jump-to-ch always starts at page 1; only the same-chapter resume keeps a page
      const qChRaw = q.get('ch');
      const qCh = qChRaw === null || qChRaw === '' ? NaN : Number(qChRaw);
      if (RESUME.mode) this.mode = RESUME.mode;
      if (RESUME.dir) this.dir = RESUME.dir;
      if (RESUME.zoom) { this.applyZoomSeed(RESUME.zoom); }
      if (Number.isFinite(qCh)) {
        this.ch = Math.max(0, qCh);
        const resume = Number(RESUME.ch);
        if (!q.get('page') && this.ch !== resume) this.page = 1; // different chapter => restart at p1
      }
      if (q.get('mode')) this.mode = q.get('mode');
      if (q.get('dir')) this.dir = q.get('dir');
      if (q.get('page')) this.page = Number(q.get('page')) || 1;
      if (q.get('zoom')) this.applyZoomSeed(q.get('zoom'));
      // chapter list is needed once for position-write id resolution; pages load per chapter
      this._chaptersReady = fetch(`/api/chapters/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}`)
        .then(r => r.json())
        .then(c => { this.chaptersList = c.data || []; })
        .catch(e => { this.err = String(e); });
      window.addEventListener('popstate', () => this._onPop());
      await this.loadChapter(this.ch, this.page, {push: false, chId: q.get('ch_id') || ''});
    },
    // fetch (or reuse prefetched) one chapter's pages and swap content in place; no document reload.
    // Navigation always targets a chapter id: the exact release the detail row
    // linked, a neighbour id, the dropdown entry, or the server echo of what it
    // resolved for a number (p.ch_id). Number-only links still work (first
    // number match) for backward compatibility.
    async loadChapter(n, page, opts) {
      const push = !opts || opts.push !== false;
      const chId = (opts && 'chId' in opts) ? opts.chId : this.chId;
      // flush any pending position POST against the OLD chapter before ch/chId change
      if (this.t) { clearTimeout(this.t); this.t = null; this.save(); }
      const token = ++this._reqToken;
      const cached = chId ? this._prefetch.get(chId) : null;
      if (chId) this._prefetch.delete(chId);
      let p;
      try {
        const suffix = chId ? `?ch_id=${encodeURIComponent(chId)}` : '';
        p = cached || await fetch(`/api/pages/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}/${n}${suffix}`).then(r => r.json());
      } catch (e) { this.err = String(e); return; }
      await this._chaptersReady;
      if (token !== this._reqToken) return; // stale response: a newer switch is in flight
      if (!p || !Array.isArray(p.pages)) { this.err = (p && p.error) || 'load failed'; return; }
      this.ch = n;
      this.page = Math.max(1, Math.min(page || 1, p.pages.length));
      this.pages = p.pages;
      this.prev_ch = p.prev_ch || 0;
      this.next_ch = p.next_ch || 0;
      this.prev_ch_id = p.prev_ch_id || '';
      this.next_ch_id = p.next_ch_id || '';
      // The server echoes the chapter id it served; trust it over the request
      // fallback so prev/prefetch keep matching the row even for a number-only URL.
      this.chId = p.ch_id || chId || '';
      this.err = '';
      this.done = false;
      this.seen = [];
      if (this.io) this.io.disconnect();
      this.io = new IntersectionObserver(
          es => es.forEach(e => e.isIntersecting && this.mark(+e.target.dataset.vi)),
          {rootMargin: '400px'});
      if (push) history.pushState({ch: n, page: this.page, chId: this.chId}, '', this.chUrl(n, this.page, this.chId));
      this.$nextTick(() => { this.observe(); this.preload(); this._resetScroll(); this._prefetchAdjacent(); });
    },
    _onPop() {
      const q = new URLSearchParams(location.search);
      const n = Number(q.get('ch'));
      const cid = q.get('ch_id') || '';
      if (!Number.isFinite(n)) return;
      const page = Number(q.get('page')) || 1;
      if (n === this.ch && cid === this.chId) { this.goto_page(page); return; }
      this.loadChapter(n, page, {push: false, chId: cid});
    },
    // prefetch the adjacent chapters' page lists (next first, then previous), keyed by chapter id
    _prefetchAdjacent() {
      const list = [];
      if (this.next_ch_id) list.push({id: this.next_ch_id, n: this.next_ch});
      if (this.prev_ch_id) list.push({id: this.prev_ch_id, n: this.prev_ch});
      for (const {id, n} of list) {
        if (this._prefetch.has(id)) continue;
        fetch(`/api/pages/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}/${n}?ch_id=${encodeURIComponent(id)}`)
          .then(r => r.json())
          .then(p => {
            if (!Array.isArray(p.pages)) return;
            this._prefetch.set(id, p);
            const keep = new Set([this.chId, this.next_ch_id, this.prev_ch_id]);
            for (const k of [...this._prefetch.keys()]) if (!keep.has(k)) this._prefetch.delete(k);
          })
          .catch(() => {});
      }
    },
    // --- layout: one screen = one view; page is the view's first page number.
    // Spread pairs (1,2) (3,4)...: odd page leads, even page joins the pair before it,
    // so the pair holding the open page is (p, p+1) for odd p, (p-1, p) for even p.
    // Tail alone when odd. Pure function of pages -- no anchor state to re-run.
    get pairing() { return this.mode === 'spread-h' || this.mode === 'spread-v'; },
    get views() {
      if (!this.pairing) return this.pages.map(u => [u]);
      const v = [];
      for (let i = 0; i < this.pages.length; i += 2) v.push(this.pages.slice(i, i + 2));
      return v;
    },
    firstOf(i) { return this.pairing ? 2 * i + 1 : i + 1; },
    viewOf(p) { return this.pairing ? Math.floor((p - 1) / 2) : p - 1; },
    cur() { return Math.max(0, Math.min(this.viewOf(this.page), this.views.length - 1)); },
    label() {
      if (!this.pages.length) return '';
      const f = this.firstOf(this.cur());
      return (this.views[this.cur()].length > 1 ? `p ${f}\u2013${f + 1}` : `p ${f}`) + ` / ${this.pages.length}p`;
    },
      applyZoomSeed(z) { // 'fit-h' | 'fit-w' | '1:1' | 'custom:NN' | 'custom'
        if (typeof z === 'string' && z.indexOf('custom:') === 0) {
          const pct = Number(z.slice(7));
          this.zoom = 'custom';
          this.zoomPct = Number.isFinite(pct) ? Math.min(400, Math.max(25, Math.round(pct))) : 100;
        } else if (z === 'custom') {
          this.zoom = 'custom';
          const pct = this.currentZoomPct();
          this.zoomPct = pct ? Math.min(400, Math.max(25, Math.round(pct))) : 100;
        } else if (z === 'fit-w' || z === 'fit-h' || z === '1:1') {
          this.zoom = z;
          // Determine the actual on‑screen zoom for this mode (fit‑w/h may already enlarge the image).
          const pct = this.currentZoomPct();
          this.zoomPct = pct ? Math.min(400, Math.max(25, Math.round(pct))) : 100;
        }
      },
      // Effective scale of the on-screen image, %: clientWidth / naturalWidth (0 if not measurable).
      currentZoomPct() {
        const img = this.$root.querySelector(`[data-vi="${this.cur()}"] img`);
        if (!img || !img.naturalWidth || !img.clientWidth) return 0;
        return Math.round((img.clientWidth / img.naturalWidth) * 100);
      },
    zoomSeed() { return this.zoom === 'custom' ? 'custom:' + Math.round(this.zoomPct) : this.zoom; },
    formatChapterLabel(c) { // 'ch N — Title'; cap keeps the open dropdown list sane (closed state truncates via CSS)
      const t = 'ch ' + c.number + (c.title ? ' \u2014 ' + c.title : '');
      return t.length > 64 ? t.slice(0, 63) + '\u2026' : t;
    },
    chUrl(n, page, chId) {
      const q = new URLSearchParams({ch: String(n), mode: this.mode, dir: this.dir, zoom: this.zoomSeed()});
      if (chId) q.set('ch_id', chId);
      if (page) q.set('page', String(page));
      return `/read/${encodeURIComponent(this.plugin)}/${encodeURIComponent(this.slug)}?${q}`;
    },
    // --- navigation
    step(d) {
      const i = this.cur() + d;
      if (i < 0) {
        if (this.prev_ch_id) {
          // land on the LAST page of the previous chapter; the prefetched list tells us the count
          const cached = this._prefetch.get(this.prev_ch_id);
          const last = cached ? cached.pages.length : 9999; // the route clamps to the last page
          this.loadChapter(this.prev_ch, last, {chId: this.prev_ch_id});
        }
        return;
      }
      if (i >= this.views.length) {
        if (this.next_ch_id) { this.loadChapter(this.next_ch, 1, {chId: this.next_ch_id}); }
        else { this.done = true; }
        return;
      }
      this.page = this.firstOf(i);
      this.persist(); // 3.3: debounced position POST on view change
      this.$nextTick(() => { this.preload(); this._resetScroll(); });
    },
    goto_page(n) {
      if (n < 1 || n > this.pages.length) return;
      this.page = this.firstOf(this.viewOf(n));
      this.persist();
      this.$nextTick(() => {
        this.preload();
        if (this.mode !== 'webtoon') { this._resetScroll(); return; }
        const el = this.$root.querySelector(`[data-vi="${this.cur()}"]`);
        if (el) el.scrollIntoView();
      });
    },
    gotoChapter(id) {
      const e = (this.chaptersList || []).find(c => String(c.id) === String(id));
      if (!e) return;
      this.loadChapter(Number(e.number), 1, {chId: String(e.id)});
    },
    wheel(e) {
      const shell = this.$root.querySelector('[data-shell]');
      if (shell && shell.scrollHeight > shell.clientHeight + 2) {
        shell.scrollTop += e.deltaY; // overlays eat native wheel; scroll shell manually
        return;
      }
      if (this.mode === 'webtoon') return; // body handles it
      if (this.zoom !== 'fit-h') return;   // no overflow left: nothing to do
      const d = e.deltaY > 8 ? 1 : e.deltaY < -8 ? -1 : 0;
      if (!d) return;
      const now = Date.now();
      if (this.wheelAt && now - this.wheelAt < 180) return; // debounce trackpad inertia
      this.wheelAt = now;
      this.step(d);
    },
    // Entering custom adopts the on-screen scale as the base, so +/- continues
    // from the size the user actually sees (fit-w on a wide screen may be ~140%).
    _adoptVisibleZoom() {
      if (this.zoom === 'custom') return;
      const pct = this.currentZoomPct();
      if (pct) this.zoomPct = Math.min(400, Math.max(25, pct));
    },
    zoomIn() { this._adoptVisibleZoom(); this.zoom = 'custom'; this.zoomPct = Math.min(400, this.zoomPct + 15); this.persist(); },
    zoomOut() { this._adoptVisibleZoom(); this.zoom = 'custom'; this.zoomPct = Math.max(25, this.zoomPct - 15); this.persist(); },
    _scrollBy(f) { window.scrollBy({top: f * window.innerHeight, behavior: 'smooth'}); },
    _resetScroll() { const shell = this.$root.querySelector('[data-shell]'); if (shell) shell.scrollTop = 0; },
    keys(e) {
      if (e.key === 'Escape' && this.done) { this.done = false; return; }
      if (/^(SELECT|INPUT|TEXTAREA)$/.test(e.target.tagName)) return; // form controls first, always
      if (e.key === '+' || e.key === '=') { e.preventDefault(); this.zoomIn(); return; }
      if (e.key === '-') { e.preventDefault(); this.zoomOut(); return; }
      if (e.key === '0') { e.preventDefault(); this.zoom = 'custom'; this.zoomPct = 100; this.persist(); return; }
      const s = this.dir === 'rtl' ? -1 : 1; // RTL flips horizontal arrows (manga reads right-to-left)
      if (this.mode === 'webtoon') {
        // ArrowLeft/Right = prev/next page everywhere; webtoon additionally scrolls natively on Up/Down
        if (e.key === 'ArrowRight') { e.preventDefault(); this.goto_page(this.page + 1); }
        else if (e.key === 'ArrowLeft') { e.preventDefault(); this.goto_page(this.page - 1); }
        else if (e.key === 'PageDown') { e.preventDefault(); this._scrollBy(0.9); }
        else if (e.key === 'PageUp') { e.preventDefault(); this._scrollBy(-0.9); }
        else if (e.key === 'Home') { e.preventDefault(); this.goto_page(1); }
        else if (e.key === 'End') { e.preventDefault(); this.goto_page(this.pages.length); }
        return;
      }
      if (e.key === 'ArrowRight') { e.preventDefault(); this.step(s); }
      else if (e.key === 'ArrowLeft') { e.preventDefault(); this.step(-s); }
      else if (e.key === 'ArrowDown' || e.key === 'PageDown') { e.preventDefault(); this.step(1); }
      else if (e.key === 'ArrowUp' || e.key === 'PageUp') { e.preventDefault(); this.step(-1); }
      else if (e.key === 'Home') { e.preventDefault(); this.goto_page(1); }
      else if (e.key === 'End') { e.preventDefault(); this.goto_page(this.pages.length); }
    },
    zoomPresetChanged() { // presets always reset; 'custom' keeps the +/- percentage
      if (this.zoom !== 'custom') this.zoomPct = 100;
      this.persist();
    },
    modeChanged() { // re-paired views: keep page, reseed observers, realign scroll
      this.persist();
      this.$nextTick(() => {
        this.observe();
        this.preload();
        if (this.mode !== 'webtoon') return;
        const el = this.$root.querySelector(`[data-vi="${this.cur()}"]`);
        if (el) el.scrollIntoView();
      });
    },
    // --- lazy loading: webtoon via observer, paged/spread via neighbours
    mark(i) { if (!this.seen.includes(i)) this.seen.push(i); },
    loaded(i) { return this.seen.indexOf(i) >= 0; },
    preload() {
      if (this.mode === 'webtoon') return;
      const i = this.cur();
      this.mark(i - 1); this.mark(i); this.mark(i + 1);
    },
    observe() { if (this.io) this.$root.querySelectorAll('[data-vi]').forEach(el => this.io.observe(el)); },
    track() { // webtoon: topmost visible row is the page
      if (this.mode !== 'webtoon') return;
      for (const el of this.$root.querySelectorAll('[data-vi]')) {
        if (el.getBoundingClientRect().bottom > 64) {
          const p = this.firstOf(+el.dataset.vi);
          if (p !== this.page) { this.page = p; this.persist(); }
          return;
        }
      }
    },
    persist() { // 3.3: ~300ms debounce -- rapid paging and mode/dir switches coalesce into one POST (the POST is also the worker nudge, D4)
      clearTimeout(this.t);
      this.t = setTimeout(() => this.save(), 300);
    },
    async save() { // fires once, with the page/mode/dir as of 300ms after the last change
      if (!this.chId) return;
      try {
        await fetch('/reader/position', {method: 'POST', headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({plugin: this.plugin, manga: this.slug, chapter: this.chId,
                                chapter_number: this.ch, page: this.page, mode: this.mode, direction: this.dir, zoom: this.zoomSeed()})});
      } catch (e) { console.warn('position', e); }
    },
    // --- chrome
    shellClass() {
      if (this.mode === 'webtoon') return 'flex flex-col items-center gap-1 py-2';
      // fit-h centers and clips to the screen; wider/taller content scrolls
      if (this.zoom === 'fit-h') return 'flex items-center justify-center h-screen';
      return 'h-screen overflow-auto py-2 text-center';
    },
    viewClass() {
      if (this.mode === 'webtoon') return 'w-full';
      const col = this.mode === 'spread-v' ? 'flex-col ' : '';
      if (this.zoom === 'fit-h') {
        return this.mode === 'spread-v'
          ? 'flex flex-col items-center justify-center gap-1 h-full'
          : 'flex items-center justify-center gap-1 h-full'; // paged + spread-h; :dir mirrors pairs
      }
      return 'inline-flex ' + col + 'items-center justify-center gap-1 min-w-full min-h-full';
    },
    imgClass() {
      if (this.mode === 'webtoon') return 'w-full';
      switch (this.zoom) {
        case 'fit-w':
          if (this.mode === 'spread-h') return 'w-1/2 h-auto object-contain';
          return 'w-full h-auto object-contain'; // paged + spread-v
        case '1:1':
          return 'max-w-none max-h-none w-auto h-auto'; // natural size, scroll
        case 'custom':
          return 'max-w-none max-h-none w-auto h-auto'; // CSS zoom (view :style) scales this
        default: // fit-h
          return 'max-h-full max-w-full object-contain';
      }
    },
  };
}
