#pragma once
// A deterministic binary min-heap priority queue keyed by next-update time, for
// event-driven adaptive scheduling. Lets different processes (e.g. fast-changing
// vs. slow species, or per-region ecosystem ticks) advance at their own cadence
// instead of every-frame uniform stepping: pop the soonest-due item, process it,
// and re-insert it at its next due time. Ties break by a stable id so a run is
// reproducible regardless of insertion order.

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace cosmos {

class Scheduler {
public:
    struct Event {
        double time = 0.0;       // due time (smaller = sooner)
        std::uint32_t id = 0;    // payload + deterministic tie-breaker
    };

    void clear() { heap_.clear(); }
    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }
    double next_time() const { return heap_.front().time; } // precondition: !empty()

    void push(double time, std::uint32_t id) {
        heap_.push_back({time, id});
        sift_up(heap_.size() - 1);
    }

    Event pop() {
        Event top = heap_.front();
        heap_.front() = heap_.back();
        heap_.pop_back();
        if (!heap_.empty()) sift_down(0);
        return top;
    }

    // Advance the schedule to `until`, invoking `fn(id)` for every due event and
    // re-inserting it at the time `fn` returns (return <= popped time to stop that
    // item). Returns the number of events processed. Deterministic given fn.
    template <typename Fn>
    int run_until(double until, Fn&& fn) {
        int processed = 0;
        while (!heap_.empty() && heap_.front().time <= until) {
            const Event ev = pop();
            const double next = fn(ev.id, ev.time);
            if (next > ev.time) push(next, ev.id);
            ++processed;
        }
        return processed;
    }

private:
    // Strict-weak ordering: earlier time first, then smaller id (determinism).
    static bool less(const Event& a, const Event& b) {
        return a.time < b.time || (a.time == b.time && a.id < b.id);
    }

    void sift_up(std::size_t i) {
        while (i > 0) {
            const std::size_t p = (i - 1) / 2;
            if (less(heap_[i], heap_[p])) { std::swap(heap_[i], heap_[p]); i = p; }
            else break;
        }
    }
    void sift_down(std::size_t i) {
        const std::size_t n = heap_.size();
        for (;;) {
            const std::size_t l = 2 * i + 1, r = 2 * i + 2;
            std::size_t m = i;
            if (l < n && less(heap_[l], heap_[m])) m = l;
            if (r < n && less(heap_[r], heap_[m])) m = r;
            if (m == i) break;
            std::swap(heap_[i], heap_[m]);
            i = m;
        }
    }

    std::vector<Event> heap_;
};

} // namespace cosmos
