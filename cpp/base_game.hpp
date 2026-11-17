#pragma once

/**
 * Bamboo Tower base game — line eval + Coin Respin scaffolding.
 *
 * Proto rules (cashfall):
 *   - 5×3 · 30 lines · wild substitutes for pays only
 *   - Feature symbols (CN/CL/RP/MU/HT/BL) break line runs
 *   - Coins lock when a Collector lands (<6 held) → Coin Respin
 *   - 6+ held coins on a spin → Cash-Fall Bonus (collector discarded)
 *   - Ordinary spin without collector: coins do not carry
 *
 * Board gen / respin loop are stubbed until weight packs are authored.
 * `evaluateLines` is implemented and matches the proto.
 */

#include "config.hpp"
#include "rng.hpp"
#include "stats.hpp"
#include "types.hpp"

#include <vector>

namespace bambootower {

struct LineEvalResult {
  double totalX = 0.0; // × total bet
  std::vector<LineWin> wins;
};

/**
 * Evaluate 30 fixed paylines on a base board (proto evaluateLines).
 * Pays are × total bet. All-wild run pays as top symbol H1 (FIRE7).
 */
inline LineEvalResult evaluateLines(const Board& board, const Config& cfg) {
  LineEvalResult out;
  const auto& lines = PAYLINES();
  for (size_t li = 0; li < lines.size(); ++li) {
    const auto& rowPerReel = lines[li];
    SymId symbols[COLS];
    for (int c = 0; c < COLS; ++c) {
      const int r = rowPerReel[static_cast<size_t>(c)];
      symbols[c] = board[static_cast<size_t>(c)][static_cast<size_t>(r)].sym;
    }

    SymId anchor = SymId::EM;
    for (int c = 0; c < COLS; ++c) {
      if (!isWild(symbols[c])) {
        anchor = symbols[c];
        break;
      }
    }
    if (anchor == SymId::EM) anchor = SymId::H1; // all wild → top pay
    if (!isPaySym(anchor)) continue;             // feature anchor → no line

    int runLen = 0;
    for (int c = 0; c < COLS; ++c) {
      if (symbols[c] == anchor || isWild(symbols[c])) ++runLen;
      else break;
    }
    if (runLen < 3) continue;

    const double payX = paytableTotalBet(anchor, runLen);
    if (payX <= 0.0) continue;

    LineWin w;
    w.payline = static_cast<int>(li) + 1;
    w.symbol = anchor;
    w.oak = runLen;
    w.payX = payX;
    out.wins.push_back(w);
    out.totalX = round2(out.totalX + payX);
  }
  (void)cfg;
  return out;
}

inline Board blankBoard(SymId fill = SymId::EM) {
  Board g{};
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      g[static_cast<size_t>(c)][static_cast<size_t>(r)].sym = fill;
  return g;
}

/** Count cells with a given symbol. */
inline int countSym(const Board& g, SymId id) {
  int n = 0;
  for (int c = 0; c < COLS; ++c)
    for (int r = 0; r < ROWS; ++r)
      if (g[static_cast<size_t>(c)][static_cast<size_t>(r)].sym == id) ++n;
  return n;
}

/**
 * Fill open cells from the active reelset. Held cells (cell.held) are skipped.
 * With blank packs this fills EM — foundation stub for later board gen.
 */
inline void fillOpenCells(Board& g, Rng& rng, const Config& cfg) {
  const Reelset& rs = pickReelset(rng, cfg.pack);
  for (int c = 0; c < COLS; ++c) {
    for (int r = 0; r < ROWS; ++r) {
      Cell& cell = g[static_cast<size_t>(c)][static_cast<size_t>(r)];
      if (cell.held) continue;
      cell.sym = pickFromReel(rng, rs, c);
      cell.coinValue = 0.0;
      cell.multHits = 0;
      cell.rolledMult = 0;
      if (isCoin(cell.sym)) cell.coinValue = pickCoinValue(rng);
      if (isMult(cell.sym)) cell.rolledMult = pickMultValue(rng);
    }
  }
}

/**
 * Boost one random eligible coin (hits < maxMultHits). Returns false if none.
 * Stub helper for Coin Respin / Cash-Fall multiplier resolution.
 */
inline bool boostRandomCoin(Rng& rng, std::vector<Coin*>& coins, int maxHits) {
  std::vector<Coin*> eligible;
  for (Coin* c : coins)
    if (c && c->hits < maxHits) eligible.push_back(c);
  if (eligible.empty()) return false;
  Coin* pick = rng.pick(eligible);
  // Caller supplies the rolled multiplier separately when wiring the loop.
  (void)pick;
  return true;
}

inline bool boostRandomCoin(Rng& rng, std::vector<Coin*>& coins, int maxHits, int mult) {
  std::vector<Coin*> eligible;
  for (Coin* c : coins)
    if (c && c->hits < maxHits) eligible.push_back(c);
  if (eligible.empty()) return false;
  Coin* pick = rng.pick(eligible);
  pick->value = round2(pick->value * static_cast<double>(mult));
  pick->hits++;
  return true;
}

/**
 * One paid base round — STUB.
 * Until reel weights are authored, board stays empty → line/feature wins 0.
 * Shape matches wild-descent playBaseRound call sites for later fill-in:
 *   1. gen board (main pack)
 *   2. line eval
 *   3. held coins / collector → Coin Respin or 6+ → Cash-Fall
 */
inline RoundResult playBaseRound(Rng& rng, const Config& cfg, Stats& st) {
  RoundResult res;
  Board g = blankBoard(SymId::EM);
  fillOpenCells(g, rng, cfg);
  res.board = g;

  const LineEvalResult lines = evaluateLines(g, cfg);
  res.lineWin = round2(lines.totalX * cfg.bet);
  st.baseWon = round2(st.baseWon + res.lineWin);

  // Feature loops (Coin Respin / Cash-Fall) stubbed — see bonus_game.hpp.
  res.total = round2(res.lineWin + res.collectWin + res.bonusWin);
  res.totalX = cfg.bet > 0.0 ? round2(res.total / cfg.bet) : 0.0;
  if (res.totalX > cfg.maxWinX) {
    res.totalX = cfg.maxWinX;
    res.total = round2(res.totalX * cfg.bet);
    res.winCapReached = true;
  }
  return res;
}

} // namespace bambootower
