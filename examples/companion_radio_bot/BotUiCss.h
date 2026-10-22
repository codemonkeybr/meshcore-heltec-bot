#pragma once

// Shared stylesheet for the status, nodes and settings pages. Served once from flash at
// /ui.css (browser-cached), so the three pages do not each carry their own copy.
// Follows the device's light/dark setting; max-width + grids make it comfortable on desktop.
static const char UI_CSS[] PROGMEM = R"CSS(
:root{color-scheme:dark light;--bg:#0f1514;--card:#162120;--line:#263230;--fg:#d6e2de;--mut:#8a9a95;--acc:#3fbaa3;--ink:#06201b;--amb:#e0c07a;--bad:#e07a7a}
@media (prefers-color-scheme:light){:root{--bg:#f1f5f4;--card:#fff;--line:#d9e3e0;--fg:#17211f;--mut:#5f6f6a;--acc:#16806c;--ink:#fff;--amb:#a9780f;--bad:#c0392b}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
.w{max-width:1100px;margin:0 auto;padding:18px 16px 56px}
a{color:var(--acc);text-decoration:none}a:hover{text-decoration:underline}
.top{display:flex;justify-content:space-between;align-items:flex-end;flex-wrap:wrap;gap:10px 24px;margin-bottom:16px}
.top h1{margin:0;font-size:1.5rem;line-height:1.2;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.sb{color:var(--mut);font-size:.85rem;margin-top:3px}
.nav{display:flex;gap:8px;flex-wrap:wrap}
.nav a{padding:7px 16px;border-radius:999px;border:1px solid var(--line);background:var(--card);color:var(--fg);font-weight:600;font-size:.88rem}
.nav a:hover{border-color:var(--acc);text-decoration:none}
.nav a.on{background:var(--acc);border-color:var(--acc);color:var(--ink)}
.pill{display:inline-block;font-size:.72rem;font-weight:600;padding:3px 10px;border-radius:999px;background:color-mix(in srgb,var(--acc) 18%,transparent);color:var(--acc);white-space:nowrap}
.pill.w{background:color-mix(in srgb,var(--amb) 20%,transparent);color:var(--amb)}
.pill.b{background:color-mix(in srgb,var(--bad) 20%,transparent);color:var(--bad)}
.g{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(min(100%,340px),1fr));align-items:start}
.sp2{grid-column:1/-1}
.kp{display:grid;gap:10px;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));margin-bottom:12px}
.k{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:11px 12px;min-width:0}
.k .l{font-size:.68rem;letter-spacing:.08em;text-transform:uppercase;color:var(--mut)}
.k .v{font-size:1.15rem;font-weight:650;font-variant-numeric:tabular-nums;margin-top:2px;white-space:nowrap}
.k .s{font-size:.76rem;color:var(--mut);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.k.bad .v{color:var(--bad)}.k.warn .v{color:var(--amb)}
.cd{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:13px 14px}
.cd h4{margin:0 0 8px;font-size:.72rem;letter-spacing:.08em;text-transform:uppercase;color:var(--mut);display:flex;justify-content:space-between;gap:8px}
.cd h4 span{letter-spacing:0;text-transform:none;font-weight:400}
.big{font-size:2rem;font-weight:700;font-variant-numeric:tabular-nums;line-height:1.1}
.big small{font-size:.9rem;font-weight:500;color:var(--mut);margin-left:4px}
.mt{position:relative;height:10px;border-radius:6px;background:var(--line);margin:12px 0 4px}
.mt i{position:absolute;left:0;top:0;bottom:0;border-radius:6px;background:var(--acc)}
.mt i.b{background:color-mix(in srgb,var(--acc) 45%,var(--card));top:3px;bottom:3px}
.mt u{position:absolute;top:-4px;bottom:-4px;width:2px;background:var(--amb)}
.ax{display:flex;justify-content:space-between;font-size:.7rem;color:var(--mut);font-variant-numeric:tabular-nums;gap:8px}
.lg{display:flex;gap:6px 16px;flex-wrap:wrap;font-size:.78rem;color:var(--mut);margin-top:8px}
.lg b{color:var(--fg);font-variant-numeric:tabular-nums}
.sw8{display:inline-block;width:8px;height:8px;border-radius:2px;margin-right:5px}
svg.sp{display:block;width:100%;height:auto;margin-top:10px}
.ln{fill:none;stroke:var(--acc);stroke-width:1.6;stroke-linejoin:round;stroke-linecap:round}
.ar{fill:var(--acc);opacity:.14}.dt{fill:var(--acc)}.gd{stroke:var(--line);stroke-width:1}
.th{stroke:var(--amb);stroke-width:1;stroke-dasharray:3 3}
.fn{display:flex;height:10px;border-radius:6px;overflow:hidden;gap:2px;margin:6px 0 4px}
.fn i{display:block;min-width:2px}
.bk{display:grid;grid-template-columns:auto 1fr auto;gap:2px 10px;align-items:center;padding:8px 0;border-top:1px solid var(--line)}
.bk:first-of-type{border-top:0;padding-top:2px}
.bk .d{width:9px;height:9px;border-radius:50%;background:var(--acc);grid-row:1/3}
.bk .d.w{background:var(--amb)}.bk .d.b{background:var(--bad)}.bk .d.o{background:var(--line)}
.bk .nm{font-weight:600;overflow-wrap:anywhere}.bk .sn{text-align:right;font-variant-numeric:tabular-nums}
.bk .sm{grid-column:2/4;font-size:.78rem;color:var(--mut)}
.kv{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,300px),1fr));gap:0 28px}
.kv div{display:flex;justify-content:space-between;gap:12px;padding:6px 0;border-bottom:1px solid var(--line);font-size:.88rem}
.kv div span:first-child{color:var(--mut);flex:none}
.kv div span:last-child{text-align:right;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.kv .bad{color:var(--bad)}.kv .warn{color:var(--amb)}
.sg{display:inline-block;vertical-align:-1px;margin-right:4px}.sg rect{fill:var(--line)}.sg rect.f{fill:var(--acc)}
.mut{color:var(--mut)}
button{font:inherit;padding:10px 16px;border:0;border-radius:9px;background:var(--acc);color:var(--ink);font-weight:600;cursor:pointer}
button.g2{background:var(--line);color:var(--fg)}
button:disabled{opacity:.6;cursor:default}
:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
@media (prefers-reduced-motion:reduce){*{transition:none!important;animation:none!important}}
)CSS";
