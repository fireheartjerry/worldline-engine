#pragma once
// A reset-per-frame bump (arena) allocator. Hands out aligned, contiguous
// scratch from a geometrically-growing block list and frees nothing until
// reset() — so per-frame / per-generation scratch (sprite lists, layout buffers,
// food-web temporaries) costs a pointer bump instead of repeated malloc/free.
// Single-threaded by design (the desktop UI + generators are single-threaded).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace cosmos {

class Arena {
public:
    explicit Arena(std::size_t initial_bytes = 64 * 1024) {
        add_block(initial_bytes < 64 ? 64 : initial_bytes);
    }

    // Allocate `count` objects of T (uninitialized), aligned for T. Never returns
    // null; grows by adding a new (geometrically larger) block on overflow.
    template <typename T>
    T* alloc(std::size_t count = 1) {
        const std::size_t bytes = count * sizeof(T);
        return static_cast<T*>(alloc_bytes(bytes, alignof(T)));
    }

    void* alloc_bytes(std::size_t bytes, std::size_t align) {
        if (bytes == 0) bytes = 1;
        Block& b = blocks_[cur_];
        std::size_t off = align_up(b.used, align);
        if (off + bytes > b.cap) {
            // Try later existing blocks (after a reset they are reusable), else grow.
            for (std::size_t k = cur_ + 1; k < blocks_.size(); ++k) {
                Block& nb = blocks_[k];
                const std::size_t noff = align_up(nb.used, align);
                if (noff + bytes <= nb.cap) {
                    cur_ = k;
                    nb.used = noff + bytes;
                    high_water_ += bytes;
                    return nb.data.get() + noff;
                }
            }
            std::size_t grow = blocks_.back().cap * 2;
            if (grow < bytes + align) grow = bytes + align;
            add_block(grow);
            cur_ = blocks_.size() - 1;
            Block& nb = blocks_[cur_];
            const std::size_t noff = align_up(nb.used, align);
            nb.used = noff + bytes;
            high_water_ += bytes;
            return nb.data.get() + noff;
        }
        b.used = off + bytes;
        high_water_ += bytes;
        return b.data.get() + off;
    }

    // Reclaim everything for reuse without freeing the backing memory.
    void reset() {
        for (Block& b : blocks_) b.used = 0;
        cur_ = 0;
        high_water_ = 0;
    }

    std::size_t block_count() const { return blocks_.size(); }
    std::size_t capacity() const {
        std::size_t c = 0;
        for (const Block& b : blocks_) c += b.cap;
        return c;
    }
    std::size_t bytes_handed_out() const { return high_water_; }

private:
    struct Block {
        std::unique_ptr<std::uint8_t[]> data;
        std::size_t cap = 0;
        std::size_t used = 0;
    };

    static std::size_t align_up(std::size_t n, std::size_t a) {
        return (n + (a - 1)) & ~(a - 1);
    }
    void add_block(std::size_t cap) {
        Block b;
        b.data.reset(new std::uint8_t[cap]);
        b.cap = cap;
        b.used = 0;
        blocks_.push_back(std::move(b));
    }

    std::vector<Block> blocks_;
    std::size_t cur_ = 0;
    std::size_t high_water_ = 0;
};

} // namespace cosmos
