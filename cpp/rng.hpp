#pragma once

#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace bambootower {

/**
 * Lightweight helper RNG (mt19937_64) — same API shape as wild-descent / Coink.
 * `nextDouble()` is the only primitive; wrappers that can answer without drawing
 * (`chance(0)`, `chance(1)`, `nextInt(<=1)`, empty containers) consume nothing.
 */
class Rng {
public:
  explicit Rng(std::uint64_t seed = 1) : eng_(seed) {}

  void reseed(std::uint64_t seed) {
    eng_.seed(seed);
    clearRecorded();
  }

  double nextDouble() { // [0,1)
    double x = uni_(eng_);
    if (recording_) recorded_.push_back(x);
    return x;
  }

  bool chance(double p) {
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    return nextDouble() < p;
  }

  int nextInt(int n) { // [0, n)
    if (n <= 1) return 0;
    return static_cast<int>(nextDouble() * n);
  }

  int weightedIndex(const std::vector<double>& weights) {
    if (weights.empty()) return -1;
    double tot = 0.0;
    for (double w : weights) tot += w;
    if (tot <= 0.0) return -1;
    double r = nextDouble() * tot;
    double acc = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) {
      acc += weights[i];
      if (r < acc) return static_cast<int>(i);
    }
    return static_cast<int>(weights.size()) - 1;
  }

  void setRecording(bool on) {
    recording_ = on;
    if (on && recorded_.capacity() < 512) recorded_.reserve(512);
  }
  void clearRecorded() { recorded_.clear(); }
  const std::vector<double>& recorded() const { return recorded_; }

  template <typename T>
  T pick(const std::vector<T>& items) {
    if (items.empty()) return T{};
    return items[static_cast<size_t>(nextInt(static_cast<int>(items.size())))];
  }

  template <typename T>
  T pickWeighted(const std::vector<std::pair<T, double>>& items) {
    if (items.empty()) return T{};
    double tot = 0.0;
    for (const auto& it : items) tot += it.second;
    if (tot <= 0.0) return T{};
    double r = nextDouble() * tot;
    double acc = 0.0;
    for (const auto& it : items) {
      acc += it.second;
      if (r < acc) return it.first;
    }
    return items.back().first;
  }

  /** Index form used by some harness checks; -1 if empty / all-zero. */
  template <typename T>
  int pickWeightedIndex(const T& items) {
    double sum = 0.0;
    for (const auto& it : items) sum += static_cast<double>(it.second);
    if (sum <= 0.0) return -1;
    double r = nextDouble() * sum;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
      r -= static_cast<double>(items[static_cast<size_t>(i)].second);
      if (r < 0.0) return i;
    }
    return static_cast<int>(items.size()) - 1;
  }

  template <typename T>
  void shuffle(std::vector<T>& a) {
    for (int i = static_cast<int>(a.size()) - 1; i > 0; --i) {
      int j = nextInt(i + 1);
      std::swap(a[i], a[j]);
    }
  }

private:
  std::mt19937_64 eng_;
  std::uniform_real_distribution<double> uni_{0.0, 1.0};
  bool recording_ = false;
  std::vector<double> recorded_;
};

} // namespace bambootower
