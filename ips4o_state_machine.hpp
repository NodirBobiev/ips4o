#pragma once

#include <iterator>
#include <stack>
#include <functional>
#include <random>
#include "ips4o/config.hpp"

namespace ips4o_sm {

enum class State {
    INIT,
    SIMPLE_CASES,
    BASE_CASE,
    SAMPLING,
    CLASSIFICATION,
    PARTITIONING,
    RECURSION,
    DONE
};


template <class Iterator, class Comparator = std::less<>, class Config = ips4o::Config<>>
class StateMachine {
public:
    using iterator = Iterator;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using difference_type = typename std::iterator_traits<Iterator>::difference_type;
    using comparator = Comparator;

    struct Task {
        iterator begin;
        iterator end;
        
        Task(iterator b, iterator e) : begin(b), end(e) {}
    };

private:
    State current_state_;
    iterator begin_;
    iterator end_;
    comparator comp_;
    std::stack<Task> task_stack_;
    std::mt19937 random_generator_;
    
    // State-specific data
    difference_type current_size_;
    bool simple_case_handled_;
    
    // Sampling state data
    bool sample_sorting_in_progress_;
    difference_type num_samples_;
    difference_type step_;
    int log_buckets_;
    int num_buckets_;
    
public:
    StateMachine(iterator begin, iterator end, comparator comp = comparator{})
        : current_state_(State::INIT)
        , begin_(begin)
        , end_(end)
        , comp_(comp)
        , current_size_(end - begin)
        , simple_case_handled_(false)
        , sample_sorting_in_progress_(false)
        , num_samples_(0)
        , step_(0)
        , log_buckets_(0)
        , num_buckets_(0) {
    }
    
    void run() {
        while (current_state_ != State::DONE) {
            step();
        }
    }
    
    void step() {
        switch (current_state_) {
            case State::INIT:
                handle_init();
                break;
            case State::SIMPLE_CASES:
                handle_simple_cases();
                break;
            case State::BASE_CASE:
                handle_base_case();
                break;
            case State::SAMPLING:
                handle_sampling();
                break;
            case State::CLASSIFICATION:
                handle_classification();
                break;
            case State::PARTITIONING:
                handle_partitioning();
                break;
            case State::RECURSION:
                handle_recursion();
                break;
            case State::DONE:
                break;
        }
    }
    
    State get_state() const { return current_state_; }
    
private:
    void handle_init() {
        current_size_ = end_ - begin_;
        current_state_ = State::SIMPLE_CASES;
    }
    
    void handle_simple_cases() {
        if (begin_ == end_) {
            simple_case_handled_ = true;
            current_state_ = State::DONE;
            return;
        }

        // If last element is not smaller than first element,
        // test if input is sorted (input is not reverse sorted)
        if (!comp_(*(end_ - 1), *begin_)) {
            if (std::is_sorted(begin_, end_, comp_)) {
                simple_case_handled_ = true;
                current_state_ = State::DONE;
                return;
            }
        } else {
            // Check whether the input is reverse sorted
            for (auto it = begin_; (it + 1) != end_; ++it) {
                if (comp_(*it, *(it + 1))) {
                    simple_case_handled_ = false;
                    current_state_ = State::BASE_CASE;
                    return;
                }
            }
            std::reverse(begin_, end_);
            simple_case_handled_ = true;
            current_state_ = State::DONE;
            return;
        }

        simple_case_handled_ = false;
        current_state_ = State::BASE_CASE;
    }
    
    void handle_base_case() {
        const difference_type base_threshold = Config::kBaseCaseMultiplier * Config::kBaseCaseSize;
        
        if (current_size_ <= base_threshold) {
            // Perform insertion sort
            if (begin_ == end_) {
                current_state_ = State::DONE;
                return;
            }
            
            for (auto it = begin_ + 1; it < end_; ++it) {
                value_type val = std::move(*it);
                if (comp_(val, *begin_)) {
                    std::move_backward(begin_, it, it + 1);
                    *begin_ = std::move(val);
                } else {
                    auto cur = it;
                    for (auto next = it - 1; comp_(val, *next); --next) {
                        *cur = std::move(*next);
                        cur = next;
                    }
                    *cur = std::move(val);
                }
            }
            current_state_ = State::DONE;
        } else {
            current_state_ = State::SAMPLING;
        }
    }
    
    void handle_sampling() {
        if (!sample_sorting_in_progress_) {
            // First time in sampling - select sample and start sorting
            const auto n = current_size_;
            log_buckets_ = Config::logBuckets(n);
            num_buckets_ = 1 << log_buckets_;
            step_ = std::max<difference_type>(1, Config::oversamplingFactor(n));
            num_samples_ = std::min(step_ * num_buckets_ - 1, n / 2);

            // Select the sample - swap random elements to front
            auto remaining = n;
            auto sample_it = begin_;
            for (difference_type i = 0; i < num_samples_; ++i) {
                const auto random_idx = std::uniform_int_distribution<difference_type>(0, --remaining)(random_generator_);
                std::swap(*sample_it, sample_it[random_idx]);
                ++sample_it;
            }

            // Push current task to stack and sort sample
            task_stack_.push(Task(begin_, end_));
            end_ = begin_ + num_samples_;
            current_size_ = num_samples_;
            sample_sorting_in_progress_ = true;
            current_state_ = State::SIMPLE_CASES;
        } else {
            // Sample is sorted, now build classifier
            auto splitter = begin_ + step_ - 1;
            
            // Choose the splitters (simplified - no actual classifier building yet)
            // Skip duplicates and fill to next power of two
            // For now, just transition to classification
            
            // Restore original task
            Task original_task = task_stack_.top();
            task_stack_.pop();
            begin_ = original_task.begin;
            end_ = original_task.end;
            current_size_ = end_ - begin_;
            sample_sorting_in_progress_ = false;
            
            current_state_ = State::CLASSIFICATION;
        }
    }
    
    void handle_classification() {
        // Classify elements into buckets
        current_state_ = State::PARTITIONING;
    }
    
    void handle_partitioning() {
        // Rearrange elements in-place
        current_state_ = State::RECURSION;
    }
    
    void handle_recursion() {
        // Add buckets to task stack for recursive processing
        // If task stack empty, we're done
        if (task_stack_.empty()) {
            current_state_ = State::DONE;
        } else {
            Task next_task = task_stack_.top();
            task_stack_.pop();
            begin_ = next_task.begin;
            end_ = next_task.end;
            current_size_ = end_ - begin_;
            
            // Check if we're returning from sample sorting
            if (sample_sorting_in_progress_) {
                current_state_ = State::SAMPLING;
            } else {
                current_state_ = State::SIMPLE_CASES;
            }
        }
    }
};

} // namespace ips4o_sm
