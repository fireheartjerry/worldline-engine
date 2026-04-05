#pragma once
#include <array>
#include <cstddef>
#include "../math/Vec2.hpp"

// Fixed-capacity ring buffer for the traced path of a pendulum bob.
// Template parameter N is the maximum number of stored entries.
// No heap allocation occurs after construction — all storage is inline.
template<std::size_t N>
class Trail {
public:
    struct Entry {
        Vec2   pos;               // world-space position of the traced bob
        double angular_velocity;  // omega used to colour this segment
    };

    void push(Vec2 pos, double angular_velocity) {
        entries[head] = {pos, angular_velocity};
        head          = (head + 1) % N;
        if (count < N) ++count;
    }

    void clear() { head = 0; count = 0; }

    bool empty() const { return count == 0; }

    std::size_t size() const { return count; }

    // Chronological access: index 0 = oldest, index (size-1) = newest.
    const Entry& at(std::size_t i) const {
        return entries[(head + N - count + i) % N];
    }

    const Entry& newest() const {
        return entries[(head + N - 1) % N];
    }

private:
    std::array<Entry, N> entries{};
    std::size_t          head  = 0;
    std::size_t          count = 0;
};
