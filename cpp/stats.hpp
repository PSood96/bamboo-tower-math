#pragma once

#include "types.hpp"

namespace bambootower {

struct HitWin {
  long long hits = 0;
  double won = 0.0; // credits
};

/**
 * Lightweight stats accumulator (wild-descent shape, Cash-Fall labels).
 * Expanded detail bands land with full engine sections later.
 */
struct Stats {
  long long spins = 0;
  double staked = 0.0;
  double stakeX = 1.0;
  double baseWon = 0.0;     // line wins
  double collectWon = 0.0;  // collector / full-board collects
  double featureWon = 0.0;  // Cash-Fall
  long long hits = 0;
  long long baseHits = 0;
  long long collectTrig = 0;
  long long coinRespinTrig = 0;
  long long cashFallTrig = 0;
  long long featureTrig = 0; // alias for cashFallTrig in reports
  double bestX = 0.0;
  double biggestFeatX = 0.0;
  HitWin maxWinCap{};

  HitWin winRange[kWinRangeCount]{};
  HitWin baseWinRange[kBaseWinRangeCount]{};

  void reset() { *this = Stats{}; }

  void recordSpin(double totalX, double baseX, double featX) {
    if (totalX > bestX) bestX = totalX;
    if (featX > biggestFeatX) biggestFeatX = featX;
    const int wi = winRangeIndex(totalX);
    if (wi >= 0 && wi < kWinRangeCount) {
      winRange[wi].hits++;
      winRange[wi].won = round2(winRange[wi].won + totalX * BET);
    }
    const int bi = baseWinRangeIndex(baseX);
    if (bi >= 0 && bi < kBaseWinRangeCount) {
      baseWinRange[bi].hits++;
      baseWinRange[bi].won = round2(baseWinRange[bi].won + baseX * BET);
    }
  }

  void finalizeStreaks() {}

  void add(const Stats& o) {
    spins += o.spins;
    staked = round2(staked + o.staked);
    baseWon = round2(baseWon + o.baseWon);
    collectWon = round2(collectWon + o.collectWon);
    featureWon = round2(featureWon + o.featureWon);
    hits += o.hits;
    baseHits += o.baseHits;
    collectTrig += o.collectTrig;
    coinRespinTrig += o.coinRespinTrig;
    cashFallTrig += o.cashFallTrig;
    featureTrig += o.featureTrig;
    if (o.bestX > bestX) bestX = o.bestX;
    if (o.biggestFeatX > biggestFeatX) biggestFeatX = o.biggestFeatX;
    maxWinCap.hits += o.maxWinCap.hits;
    maxWinCap.won = round2(maxWinCap.won + o.maxWinCap.won);
    for (int i = 0; i < kWinRangeCount; ++i) {
      winRange[i].hits += o.winRange[i].hits;
      winRange[i].won = round2(winRange[i].won + o.winRange[i].won);
    }
    for (int i = 0; i < kBaseWinRangeCount; ++i) {
      baseWinRange[i].hits += o.baseWinRange[i].hits;
      baseWinRange[i].won = round2(baseWinRange[i].won + o.baseWinRange[i].won);
    }
  }
};

} // namespace bambootower
