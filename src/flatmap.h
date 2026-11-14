// A hash table from 64-bit keys to ints, with open addressing and linear probing, that is emptied in
// constant time (a generation number marks the live slots) and keeps its memory between uses.
// Used where a std::map or std::unordered_map was rebuilt every frame (who stands where) or for every
// route search (the nodes seen).
#pragma once
#include <cstdint>
#include <vector>

class FlatIntMap {
public:
    explicit FlatIntMap(size_t capacity = 64) { allocate(roundUp(capacity)); }
    size_t size() const { return n_; }
    void clear() {
        n_ = 0;
        if (++gen_ == 0) { for (Slot &s : t_) s.gen = 0; gen_ = 1; }
    }
    // the value for a key, or nullptr
    int *find(int64_t k) {
        for (size_t i = home(k);; i = (i + 1) & mask_) {
            Slot &s = t_[i];
            if (s.gen != gen_) return nullptr;
            if (s.key == k) return &s.val;
        }
    }
    const int *find(int64_t k) const { return const_cast<FlatIntMap *>(this)->find(k); }
    size_t count(int64_t k) const { return find(k) ? 1 : 0; }
    // the value for a key, made (0) if there is none
    int &operator[](int64_t k) {
        for (size_t i = home(k);; i = (i + 1) & mask_) {
            Slot &s = t_[i];
            if (s.gen != gen_) {
                if ((n_ + 1) * 2 > t_.size()) { grow(); return (*this)[k]; }
                s.gen = gen_; s.key = k; s.val = 0; n_++;
                return s.val;
            }
            if (s.key == k) return s.val;
        }
    }
    void insert(int64_t k) { (*this)[k]; }
    // true if there was one
    bool erase(int64_t k) {
        size_t i = home(k);
        for (;; i = (i + 1) & mask_) {
            if (t_[i].gen != gen_) return false;
            if (t_[i].key == k) break;
        }
        // backward-shift deletion: move later members of the cluster up to fill the hole
        size_t hole = i;
        for (size_t j = (i + 1) & mask_; t_[j].gen == gen_; j = (j + 1) & mask_) {
            size_t h = home(t_[j].key);
            // t_[j] may move to the hole unless its home lies cyclically in (hole, j]
            bool between = hole <= j ? (h > hole && h <= j) : (h > hole || h <= j);
            if (!between) { t_[hole] = t_[j]; hole = j; }
        }
        t_[hole].gen = 0;
        n_--;
        return true;
    }

private:
    struct Slot { int64_t key; int32_t val; uint32_t gen; };
    std::vector<Slot> t_;
    size_t mask_ = 0, n_ = 0;
    uint32_t gen_ = 1;
    static size_t roundUp(size_t c) { size_t n = 16; while (n < c * 2) n *= 2; return n; }
    void allocate(size_t n) { t_.assign(n, Slot{0, 0, 0}); mask_ = n - 1; n_ = 0; gen_ = 1; }
    size_t home(int64_t k) const { return (size_t)(((uint64_t)k * 0x9E3779B97F4A7C15ull) >> 29) & mask_; }
    void grow() {
        std::vector<Slot> old;
        old.swap(t_);
        uint32_t g = gen_;
        allocate(old.size() * 2);
        for (const Slot &s : old)
            if (s.gen == g) (*this)[s.key] = s.val;
    }
};
