# Bamboo Tower — math

Coin Collector · Cash-Fall Towers math pack: modular C++ simulator (wild-descent layout), HTML proto reference, and design notes.

Foundation stub — weight packs empty until authored; line eval + harness are live.

## Layout

```
.cursor/rules/   Agent rules (symbol list, no weight edits, no auto-commits, sanity)
cpp/             Modular C++ sim — main.cpp + headers + sanity harness
scripts/         run_sanity_checks.sh
docs/            Design notes
proto/           HTML prototype reference
```

## Quick start

```bash
# Sanity checks (required after config / strip edits)
./scripts/run_sanity_checks.sh

# or from cpp/
cd cpp && c++ -std=c++17 -O2 -pthread -o test_bamboo_tower test_bamboo_tower.cpp && ./test_bamboo_tower

# C++ sim (foundation — empty packs → RTP 0)
cd cpp && c++ -std=c++17 -O2 -pthread -o bamboo-tower main.cpp
./bamboo-tower --mode main --spins 10000
./bamboo-tower --mode respin --spins 10000
./bamboo-tower --mode cashfall --spins 10000
# → stdout CSV + results/bamboo_tower_{mode}_stats.txt (--out - = stdout only)

# HTML proto
open proto/coin-collector-cashfall-proto7-visual.html
```

## Game summary (from proto)

| Topic | Rule |
|-------|------|
| Board | Base **5×3** · **30** fixed L→R paylines |
| Pays | H1–H3 / L1–L5 · wild substitutes · feature symbols break runs |
| Features | Coin, Collector, Replicator, Multiplier, Heart (bonus), Blank (respin) |
| Coin Respin | Collector lands with **&lt;6** held → 3 respins (reset on coin/mult) |
| Cash-Fall | **6+** held coins → per-reel towers heights **3–7** · 3 spins/reel |
| Modes | `main` · `respin` (coin-range pack) · `cashfall` (bonus path) |

## Sanity checks

Same shape as wild-descent (`CHECK` asserts, no GoogleTest):

- Symbol-list / strip-token hygiene (odd landable tokens must be empty)
- Reelset registration + non-negative weights
- Blank mode weight-pack structure
- Proto paytable / 30 paylines / line-eval smoke
- RNG + coin/mult table smoke
- Cash-Fall / base-round stubs return zero under empty packs

## Agent rules (must follow)

| Rule | File |
|------|------|
| Reel / weight tokens ⊆ symbol list | `.cursor/rules/reelstrip-symbol-list.mdc` |
| Never change weight tables unless asked | `.cursor/rules/no-weight-table-edits.mdc` |
| Never commit / push unless asked | `.cursor/rules/no-automatic-commits.mdc` |
| Keep + run regular sanity checks | `.cursor/rules/sanity-checks.mdc` |

## Status

C++ foundation ported from wild-descent and rebranded for Cash-Fall / Bamboo Tower.
Board gen, Coin Respin loop, and Cash-Fall step loop are **stubbed** for section-by-section work.
Weight packs intentionally empty (no automatic retunes).
