// Fixture: happy path + budget spin, driven from tests/test_plugin.cpp.

// Mangafire VRF tables -- same vectors as tests/test_vrf.cpp / lua fixture crypto.
const STAGES = [
  { iv: 0x5a, key: "0Ec58JOY3uBzJK9m3zqIOpdlF7UFiax9DmA=", tbl: "yINlmUNho8VYJT+ibTIP+9ESiULpVEtMOoD6U6lRE0R/xwXo/Xp9NrUgC4cw/Lmo33vUyjUE40kUoEWIr/fxfNNcq2s79ShQ5NhNrFnJ4hXPwOu/SuXzIbuTQKGFvfm08E9jvCfqAtoDqvQq3dVWPQFmJjgvkISBeXY3BgANR+yVnjGbcxZ47d6kLNfZPIayTq3/YGySb1KuVZodWp/WGNAO5pfMcpaK53Hhs0allBszaMaxuouOwdxbwgxIw6YunSsXjI05Yi0j9j4eHKfSXR8Ifo/Od+8iamRfCXTyvm7NGRGYdcQ0ywcK/u6RXhrbcCm4t2eCtrDgQVecJGkQ+A==" },
  { iv: 0x35, key: "AAdjb1iPY8CiDmq9H34tKTBF8a3oDQ==", tbl: "IUFltCxD3Oc2cwCgkJffthaOg9cgPUb0LgW6H/VtfcF0kc5F25t+aWj6JH9VOhOaY0rAFdUxlDnl5BLNvwEJvQtP5qcw7vdb/K+chnbwnspSHT8mz5lqwz41TezG0hkO06FTjJZhsyNuFLDpD2ZZxQj/QIRcF90zpmQ7Byu483WsQqUE0C342HL+JXngRB6fRzxRyVTaKu83h7UYTJ0QMt6ixFh6S3F8gqkKwrGTL3jHNBsD45UnifK8+RGtishQV2K3rujLKEkiZxpr2dYcudFW4oFsDKhad3CLBvuyTqsCo4B7mL5IKQ1vXo/MOOvq1I1d8ar9X6Ttu5KF4fZgiA==" },
  { iv: 0xba, key: "DELOJgPsVaCcblDtTGMdHzM=", tbl: "NQHlu1/wVO5EmkwQymF810qqY2xG1k2obcas4Z9mCsPEIFl9pRIjFxbJ7ybMHbBckT5Ton85E0FOeHezbh/mjlEYpmpnlXOS8dgrqeq2KfxImTh1YK9y0PeMNhzA1OQzSY9brYOJq/l2QnE/hwOeZIhPixVSKIUlDb5vLcH6RWKxkIEMuP0bDwIqQ71AJJaEaMJL7A6YtyIwoRT+L5v4aZzodN/0+3nOGsfblFjgxSfPzVDjNFeNl5P26+kEC/8AHgdrpAbt3hHz3HrRN1Y6e+JHgF7ncFWnoF0y3THL1S71WgWGCa6KtSzTCCG58n68nTyj2T3Sshk7utqCtMi/ZQ==" },
];

function handle(env) {
  if (env.method === "ping") {
    return { ok: true, data: { js: "pong" } };
  }
  if (env.method === "spin") {
    while (true) {} // interrupt handler ends the call: ok:false
  }
  if (env.method === "url") {
    const s = "a b&c=d%?";
    return {
      ok: true,
      data: {
        enc: host.url_encode(s),
        dec: host.url_decode("a%20b%26c%3Dd%25%3F"),
        rt: host.url_decode(host.url_encode(env.params.s)),
      },
    };
  }
  if (env.method === "urlbad") {
    try {
      host.url_encode(42); // non-string arg: TypeError per host surface
      return { ok: false, error: "url_encode accepted a number" };
    } catch (e) {
      return { ok: true, data: { msg: String(e) } };
    }
  }
  if (env.method === "vrf") {
    const sig = host.crypto.vrf_sign(env.params.path, env.params.params || {}, STAGES);
    return sig === null ? { ok: false, error: "vrf_sign returned null" } : { ok: true, data: { sig } };
  }
  if (env.method === "vrfbad") {
    return {
      ok: true,
      data: {
        badkey: host.crypto.vrf_sign("/titles", {}, [{ iv: 1, key: "!!!", tbl: "AAAA" }]),
        nested: host.crypto.vrf_sign("/titles", { a: { b: 1 } }, STAGES),
        noconf: host.config.get("x"), // jsfix ships no config.json: null, no crash
      },
    };
  }
  return { ok: false, error: "unknown method" };
}
