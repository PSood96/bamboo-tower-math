# Bamboo Tower — base notes

Design reference: `proto/coin-collector-cashfall-proto7-visual.html`
(Coin Collector · Cash-Fall Towers — placeholder prototype, not tuned).

## Layout

- Base board: **5 reels × 3 rows**
- **30** fixed paylines, left → right from reel 1
- Cash-Fall towers: per-reel heights `{3,4,5,6,7}` → bonus grid **7** rows tall

## Symbol map (math ↔ proto)

| Math | Proto | Role |
|------|-------|------|
| H1 | FIRE7 | Top high |
| H2 | BELL | High |
| H3 | CHR | High |
| L1–L5 | A K Q J 10 | Lows |
| WW / W | WILD | Substitute |
| CN | COIN | Held / tower coin |
| CL | COLL | Collector |
| RP | REPL | Replicator (bonus) |
| MU | MULT | Multiplier (respin/bonus) |
| HT | HEART | Revive (bonus) |
| BL | BLANK | Respin open cell |
| EM | — | Engine empty (not a strip token) |

## Modes (C++ `SimMode`)

| Mode | Pack | Role |
|------|------|------|
| `main` | Main1 | Ordinary base spin |
| `respin` | Respin1 | Coin-range pool (blank/coin/mult/collector) |
| `cashfall` | Bonus1 | Cash-Fall buy/force path |

Weight pick tables are **empty** in the foundation (preference: no weight edits until asked).

## Feature flow (proto)

1. **Base spin** — fill open cells; evaluate 30 lines (wild sub; feature breaks run).
2. **Coins** lock only if a **Collector** lands with **&lt;6** held → **Coin Respin**.
3. Else if **6+** held at spin end → **Cash-Fall Bonus** (same-spin collector discarded).
4. Else coins from that spin clear (do not carry).
5. **Coin Respin** — open cells use respin pool; 3 respins reset on coin/mult; armed collectors sweep held total at end; full board → double (+ collectors).
6. **Cash-Fall** — carry held into same-reel towers; per-reel 3 spins; COLL/REPL/HEART/MULT; leftover tower coins forfeited when all reels dead.

## Base board-gen order

1. Empty board  
2. Roll collector via `Config::pCollectorLand` (not a reel weight table)  
3. Place one collector on a random empty cell  
4. Roll coin count 0–6 via `Config::coinCountW`  
5. Place coins on empty cells only; cash value integer `[coinValueMin, coinValueMax]` (default 1–100)  
6. Fill remaining EM with **pays + wild** from reelsets (`pickNormalFromReel`)

Defaults: `pCollectorLand=0`, all coin-count mass on 0 → no features until knobs are set. Empty `REELSETS()` weights use **`pickScaffoldNormal`** (proto BASE_POOL pay+wild ratios, outside weight tables) so main-mode sims produce line wins before packs are authored.

## Implemented vs stubbed

| Piece | Status |
|-------|--------|
| types / RNG / config shape / CLI / stats CSV | Ported |
| 30 paylines + line eval + proto paytable | Implemented |
| Reelset registration / odd-token hygiene | Implemented (empty packs) |
| Base board gen (collector → coins → normal fill) | **Implemented** (knobs + scaffold fill) |
| Scaffold normal fill (`pickScaffoldNormal`) | **Implemented** — not a REELSETS weight table |
| Reel weight packs | Stub (zeros — do not edit unless asked) |
| Coin Respin loop | Stub |
| Cash-Fall step loop | Stub (`playCashFall` → 0) |

## Sanity

```bash
./scripts/run_sanity_checks.sh
```
