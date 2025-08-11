#pragma once

#include <iterator>
#include <stack>
#include <functional>
#include <random>
#include <vector>
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

    enum class TaskType {
        MAIN_SORT,
        SAMPLE_SORT
    };
    
    struct Task {
        iterator begin;
        iterator end;
        TaskType type;
        
        // Context-specific data to avoid corruption in nested operations
        std::vector<value_type> splitters;
        std::vector<difference_type> bucket_counts;
        std::vector<difference_type> bucket_starts;
        difference_type num_samples;
        difference_type step;
        int log_buckets;
        int num_buckets;
        
        Task(iterator b, iterator e, TaskType t = TaskType::MAIN_SORT) 
            : begin(b), end(e), type(t), num_samples(0), step(0), log_buckets(0), num_buckets(0) {}
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
    
    // Current task context (to avoid using member variables that could be corrupted)
    Task* current_task_context_;
    
public:
    StateMachine(iterator begin, iterator end, comparator comp = comparator{})
        : current_state_(State::INIT)
        , begin_(begin)
        , end_(end)
        , comp_(comp)
        , current_size_(end - begin)
        , simple_case_handled_(false)
        , current_task_context_(nullptr) {
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
        // Check if we're returning from sample sorting
        if (!task_stack_.empty() && task_stack_.top().type == TaskType::SAMPLE_SORT) {
            // Sample is sorted, now build classifier
            Task& context = task_stack_.top();
            context.splitters.clear();
            auto splitter = begin_ + context.step - 1;
            
            // Choose the splitters from sorted sample
            context.splitters.push_back(*splitter);
            for (int i = 2; i < context.num_buckets; ++i) {
                splitter += context.step;
                // Skip duplicates
                if (comp_(context.splitters.back(), *splitter)) {
                    context.splitters.push_back(*splitter);
                }
            }
            
            // Initialize bucket counts
            context.bucket_counts.assign(context.num_buckets, 0);
            
            // Restore original task
            begin_ = context.begin;
            end_ = context.end;
            current_size_ = end_ - begin_;
            current_task_context_ = &context;
            
            current_state_ = State::CLASSIFICATION;
        } else {
            // First time in sampling - select sample and start sorting
            const auto n = current_size_;
            Task new_task(begin_, end_, TaskType::SAMPLE_SORT);
            new_task.log_buckets = Config::logBuckets(n);
            new_task.num_buckets = 1 << new_task.log_buckets;
            new_task.step = std::max<difference_type>(1, Config::oversamplingFactor(n));
            new_task.num_samples = std::min(new_task.step * new_task.num_buckets - 1, n / 2);

            // Select the sample - swap random elements to front
            auto remaining = n;
            auto sample_it = begin_;
            for (difference_type i = 0; i < new_task.num_samples; ++i) {
                const auto random_idx = std::uniform_int_distribution<difference_type>(0, --remaining)(random_generator_);
                std::swap(*sample_it, sample_it[random_idx]);
                ++sample_it;
            }

            // Push current task to stack and sort sample
            task_stack_.push(new_task);
            end_ = begin_ + new_task.num_samples;
            current_size_ = new_task.num_samples;
            current_state_ = State::SIMPLE_CASES;
        }
    }
    
    void handle_classification() {
        if (!current_task_context_) return;
        
        // Classify each element into its target bucket
        for (auto it = begin_; it != end_; ++it) {
            // Find which bucket this element belongs to
            int bucket = 0;
            for (size_t i = 0; i < current_task_context_->splitters.size(); ++i) {
                if (comp_(*it, current_task_context_->splitters[i])) {
                    bucket = static_cast<int>(i);
                    break;
                }
                bucket = static_cast<int>(i + 1);
            }
            
            // Count elements per bucket
            if (bucket < static_cast<int>(current_task_context_->bucket_counts.size())) {
                current_task_context_->bucket_counts[bucket]++;
            }
        }
        
        current_state_ = State::PARTITIONING;
    }
    
    void handle_partitioning() {
        if (!current_task_context_) return;
        
        // Calculate bucket start positions
        current_task_context_->bucket_starts.clear();
        current_task_context_->bucket_starts.resize(current_task_context_->bucket_counts.size() + 1);
        current_task_context_->bucket_starts[0] = 0;
        for (size_t i = 0; i < current_task_context_->bucket_counts.size(); ++i) {
            current_task_context_->bucket_starts[i + 1] = current_task_context_->bucket_starts[i] + current_task_context_->bucket_counts[i];
        }
        
        // Simple in-place partitioning using stable_partition
        auto current_pos = begin_;
        for (size_t bucket = 0; bucket < current_task_context_->splitters.size() + 1; ++bucket) {
            if (current_task_context_->bucket_counts[bucket] == 0) continue;
            
            auto bucket_end = std::stable_partition(current_pos, end_, 
                [this, bucket](const value_type& val) {
                    // Check if element belongs to current bucket
                    if (bucket == 0) {
                        return current_task_context_->splitters.empty() || comp_(val, current_task_context_->splitters[0]);
                    } else if (bucket <= current_task_context_->splitters.size()) {
                        bool greater_than_prev = bucket == 1 || !comp_(val, current_task_context_->splitters[bucket - 1]);
                        bool less_than_curr = bucket > current_task_context_->splitters.size() || comp_(val, current_task_context_->splitters[bucket - 1]);
                        return greater_than_prev && less_than_curr;
                    }
                    return false;
                });
            
            current_pos = bucket_end;
        }
        
        current_state_ = State::RECURSION;
    }
    
    void handle_recursion() {
        if (current_task_context_) {
            // Add buckets to task stack for recursive processing
            const difference_type base_threshold = Config::kBaseCaseMultiplier * Config::kBaseCaseSize;
            
            // Add non-trivial buckets to task stack
            for (size_t i = 0; i < current_task_context_->bucket_starts.size() - 1; ++i) {
                const auto bucket_size = current_task_context_->bucket_starts[i + 1] - current_task_context_->bucket_starts[i];
                if (bucket_size > base_threshold) {
                    auto bucket_begin = begin_ + current_task_context_->bucket_starts[i];
                    auto bucket_end = begin_ + current_task_context_->bucket_starts[i + 1];
                    task_stack_.push(Task(bucket_begin, bucket_end, TaskType::MAIN_SORT));
                }
            }
            
            // Pop the completed sample sort task
            task_stack_.pop();
            current_task_context_ = nullptr;
        }
        
        // Process next task or finish
        if (task_stack_.empty()) {
            current_state_ = State::DONE;
        } else {
            Task next_task = task_stack_.top();
            task_stack_.pop();
            begin_ = next_task.begin;
            end_ = next_task.end;
            current_size_ = end_ - begin_;
            
            // Check task type to determine next state
            if (next_task.type == TaskType::SAMPLE_SORT) {
                current_state_ = State::SAMPLING;
            } else {
                current_state_ = State::SIMPLE_CASES;
            }
        }
    }
};

} // namespace ips4o_sm
