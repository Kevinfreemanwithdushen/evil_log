#include "log.h"

namespace Argus
{

std::unordered_set<uint64_t> buffer_maintaner::judge_book_;
std::atomic_flag buffer_maintaner::lock_{ATOMIC_FLAG_INIT};
thread_local circle_buffer *evil_logger::pbuffer_{nullptr};
thread_local circle_buffer evil_logger::buffer{0};

bool buffer_maintaner::useful(void *addr)
{
    bool res = true;
    uint64_t val = reinterpret_cast<uint64_t>(addr);
    while (unlikely(lock_.test_and_set(std::memory_order_acquire)))
        ;
    if (judge_book_.find(val) != judge_book_.end()) res = false;
    lock_.clear();
    return res;
}

void buffer_maintaner::erase(void *addr)
{
    while (unlikely(lock_.test_and_set(std::memory_order_acquire)))
        ;
    judge_book_.erase(reinterpret_cast<uint64_t>(addr));
    lock_.clear();
}

void buffer_maintaner::maintan(void *addr)
{
    while (!((circle_buffer *)addr)->empty()) {
    }
    while (unlikely(lock_.test_and_set(std::memory_order_acquire)))
        ;
    judge_book_.insert(reinterpret_cast<uint64_t>(addr));
    lock_.clear();
}

circle_buffer::~circle_buffer()
{
    buffer_maintaner::maintan(this);
}

char *circle_buffer::peek(uint64_t *bytes_available)
{
    char *cachedProducerPos = procucer_pos_;
    if (cachedProducerPos < consumer_pos_) {
        lfence();
        *bytes_available = real_buff_end_pos_ - consumer_pos_;

        if (likely(*bytes_available > 0)) {
            return consumer_pos_;
        }

        // Roll over
        consumer_pos_ = buff_;
    }

    *bytes_available = cachedProducerPos - consumer_pos_;
    return consumer_pos_;
}

char *circle_buffer::busy_alloc(unsigned nbytes)
{
    const char *endOfBuffer = buff_ + BUFFER_SIZE;
    while (available_bytes_ <= nbytes) {
        char *cachedConsumerPos = consumer_pos_;

        if (cachedConsumerPos <= procucer_pos_) {
            available_bytes_ = endOfBuffer - procucer_pos_;

            if (available_bytes_ > nbytes) break;

            real_buff_end_pos_ = procucer_pos_; // wrap around

            // Prevent the roll over if it overlaps the two positions because
            // that would imply the buffer is completely empty when it's not.
            if (unlikely(cachedConsumerPos != buff_)) {
                // prevents procucer_pos_ from updating before real_buff_end_pos_
                sfence();
                procucer_pos_ = buff_;
                available_bytes_ = cachedConsumerPos - procucer_pos_;
            }
        } else {
            available_bytes_ = cachedConsumerPos - procucer_pos_;
        }
    }

    return procucer_pos_;
}

} // namespace Argus