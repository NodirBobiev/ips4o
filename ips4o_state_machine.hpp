#pragma once

#include <iterator>
#include <stack>
#include <functional>
#include <random>
#include <vector>
#include <fstream>
#include "ips4o/config.hpp"

using namespace std;

// Re-enable logging as requested
// #define LOG_FILENAME "logs_v3.txt"

#ifdef LOG_FILENAME
    ofstream logFile(LOG_FILENAME);

    // Regular argument
    template<bool _, typename T>
    void LogImpl(T&& arg) {
        logFile << std::forward<T>(arg);
    }

    // Variadic unpacking
    template<bool space, typename T, typename... Args>
    void LogImpl(T&& first, Args&&... rest) {
        logFile << std::forward<T>(first);
        if constexpr(space && sizeof...(rest) > 0){
            logFile<<' ';
        }
        LogImpl<space>(std::forward<Args>(rest)...);
    }
    #define LOG(...) LogImpl<false>(__VA_ARGS__)
    #define LOGW(...)LogImpl<true>(__VA_ARGS__)
#else
    #define LOG(...)
    #define LOGW(...)
#endif

namespace ips4o_sm {

enum class State {
    SIMPLE_CASES,
    BASE_CASE,
    SAMPLE_SELECT,
    SAMPLE_SORTED,
    CLASSIFY,
    PARTITION,
    RECURSE
};

std::vector<std::string> state_names = {
    "SIMPLE_CASES",
    "BASE_CASE",
    "SAMPLE_SELECT",
    "SAMPLE_SORTED",
    "CLASSIFY",
    "PARTITION",
    "RECURSE"
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
        State state;  // Each task has its own state!
        
        // Context-specific data to avoid corruption in nested operations
        std::vector<value_type> splitters;
        std::vector<difference_type> bucket_counts;
        std::vector<difference_type> bucket_starts;
        difference_type num_samples;
        difference_type step;
        int log_buckets;
        int num_buckets;
        
        Task(iterator b, iterator e, State s = State::SIMPLE_CASES) 
            : begin(b), end(e), state(s), num_samples(0), step(0), log_buckets(0), num_buckets(0) {}
    };

private:
    comparator comp_;
    std::stack<Task> task_stack_;
    std::mt19937 random_generator_;
    
public:
    StateMachine(iterator begin, iterator end, comparator comp = comparator{})
        : comp_(comp), random_generator_(std::random_device{}()) {
        // Push initial task to stack
        task_stack_.push(Task(begin, end, State::SIMPLE_CASES));
        LOGW("StateMachine created with", end - begin, "elements\n");
    }
    
    void run() {
        while (!task_stack_.empty()) {
            step();
        }
    }
    
    void step() {
        if (task_stack_.empty()) return;
        
        Task& current_task = task_stack_.top();
        LOGW(state_names[static_cast<int>(current_task.state)], "elements:", current_task.end - current_task.begin, "begin:", *current_task.begin, "end:", *current_task.end, "\n");

        switch (current_task.state) {
            case State::SIMPLE_CASES:
                handle_simple_cases();
                break;
            case State::BASE_CASE:
                handle_base_case();
                break;
            case State::SAMPLE_SELECT:
                handle_sample_select();
                break;
            case State::SAMPLE_SORTED:
                handle_sample_sorted();
                break;
            case State::CLASSIFY:
                handle_classify();
                break;
            case State::PARTITION:
                handle_partition();
                break;
            case State::RECURSE:
                handle_recurse();
                break;
        }
        LOGW("------------------------------\n");
    }

    bool is_done() const {
        return task_stack_.empty();
    }
    
private:
    void handle_simple_cases() {
        Task& current_task = task_stack_.top();        
        if (current_task.begin == current_task.end) {
            task_stack_.pop();
            LOGW(" -> DONE (begin == end)\n");
            return;
        }

        // If last element is not smaller than first element,
        // test if input is sorted (input is not reverse sorted)
        if (!comp_(*(current_task.end - 1), *current_task.begin)) {
            if (std::is_sorted(current_task.begin, current_task.end, comp_)) {
                task_stack_.pop();
                LOGW(" -> DONE (sorted)\n");
                return;
            }
        } else {
            // Check whether the input is reverse sorted
            for (auto it = current_task.begin; (it + 1) != current_task.end; ++it) {
                if (comp_(*it, *(it + 1))) {
                    current_task.state = State::BASE_CASE;
                    LOGW(" -> BASE_CASE\n");
                    return;
                }
            }
            std::reverse(current_task.begin, current_task.end);
            task_stack_.pop();
            LOGW(" -> DONE (reverse sorted)\n");
            return;
        }

        current_task.state = State::BASE_CASE;
        LOGW(" -> BASE_CASE\n");
    }
    
    void handle_base_case() {
        Task& current_task = task_stack_.top();
        const difference_type base_threshold = Config::kBaseCaseMultiplier * Config::kBaseCaseSize;
        if (current_task.end - current_task.begin <= base_threshold) {
            // Perform insertion sort
            if (current_task.begin == current_task.end) {
                task_stack_.pop();
                LOGW(" -> DONE (begin == end)\n");
                return;
            }
            
            for (auto it = current_task.begin + 1; it < current_task.end; ++it) {
                value_type val = std::move(*it);
                if (comp_(val, *current_task.begin)) {
                    std::move_backward(current_task.begin, it, it + 1);
                    *current_task.begin = std::move(val);
                } else {
                    auto cur = it;
                    for (auto next = it - 1; comp_(val, *next); --next) {
                        *cur = std::move(*next);
                        cur = next;
                    }
                    *cur = std::move(val);
                }
            }
            task_stack_.pop();
            LOGW(" -> DONE (insertion sort)\n");
        } else {
            current_task.state = State::SAMPLE_SELECT;
            LOGW(" -> SAMPLE_SELECT\n");
        }
    }
    
    void handle_sample_select() {
        Task& current_task = task_stack_.top();
        
        // First time in sampling - select sample and start sorting
        const auto n = current_task.end - current_task.begin;
        Task context_task = current_task;  // Save current task context
        context_task.state = State::SAMPLE_SORTED;  // When we return, go to SAMPLE_SORTED
        context_task.log_buckets = Config::logBuckets(n);
        context_task.num_buckets = 1 << context_task.log_buckets;
        context_task.step = std::max<difference_type>(1, Config::oversamplingFactor(n));
        context_task.num_samples = std::min(context_task.step * context_task.num_buckets - 1, n / 2);

        LOGW("log_buckets:", context_task.log_buckets, "num_buckets:", context_task.num_buckets, "step:", context_task.step, "num_samples:", context_task.num_samples, "\n");

        // Select the sample - swap random elements to front
        auto remaining = n;
        auto sample_it = current_task.begin;
        for (difference_type i = 0; i < context_task.num_samples; ++i) {
            const auto random_idx = std::uniform_int_distribution<difference_type>(0, --remaining)(random_generator_);
            std::swap(*sample_it, *(sample_it + random_idx));
            ++sample_it;
        }
        
        
        // Replace current task with context and push sample sort task
        task_stack_.top() = context_task;
        task_stack_.push(Task(current_task.begin, current_task.begin + context_task.num_samples, State::SIMPLE_CASES));
        
        LOG("samples: ");
        for (auto it = current_task.begin; it != current_task.begin + context_task.num_samples; ++it) {
            LOG(*it, " ");
        }
        LOG("\n");
        LOGW(" -> SIMPLE_CASES (sample sort)\n");
    }
    
    void handle_sample_sorted() {        
        // Get the context task (now on top)
        Task& context = task_stack_.top();
        
        // Build splitters from sorted sample
        context.splitters.clear();
        
        if (context.num_samples > 0 && context.step > 0) {
            auto sample_end = context.begin + context.num_samples;
            auto splitter = context.begin + context.step - 1;
            
            while (splitter < sample_end && context.splitters.size() < static_cast<size_t>(context.num_buckets - 1)) {
                context.splitters.push_back(*splitter);
                splitter += context.step;
                
                // Skip duplicates
                while (splitter < sample_end && 
                       !context.splitters.empty() && 
                       !comp_(context.splitters.back(), *splitter)) {
                    splitter += context.step;
                }
            }
        }

        // Initialize bucket counts and transition to classification
        context.bucket_counts.assign(context.splitters.size() + 1, 0);
        context.state = State::CLASSIFY;
        LOG("splitters: ");
        for (auto splitter : context.splitters) {
            LOG(splitter, " ");
        }
        LOG("\n");
        LOGW(" -> CLASSIFY\n");
    }
    
    void handle_classify() {
        Task& current_task = task_stack_.top();
        // Classify each element into its target bucket
        for (auto it = current_task.begin; it != current_task.end; ++it) {
            // Find which bucket this element belongs to
            int bucket = 0;
            for (size_t i = 0; i < current_task.splitters.size(); ++i) {
                if (comp_(*it, current_task.splitters[i])) {
                    bucket = static_cast<int>(i);
                    break;
                }
                bucket = static_cast<int>(i + 1);
            }
            
            // Count elements per bucket
            if (bucket < static_cast<int>(current_task.bucket_counts.size())) {
                current_task.bucket_counts[bucket]++;
            }
        }
        
        current_task.state = State::PARTITION;
        LOG("bucket_counts: ");
        for (auto bucket : current_task.bucket_counts) {
            LOG(bucket, " ");
        }
        LOG("\n");
        LOGW(" -> PARTITION\n");
    }
    
    void handle_partition() {
        Task& current_task = task_stack_.top();
        
        LOGW("PARTITION: Starting with ", current_task.splitters.size(), " splitters\n");
        
        // Use in-place partitioning with std::partition for better performance (Issue #5)
        current_task.bucket_starts.clear();
        current_task.bucket_starts.push_back(0);
        
        auto current_begin = current_task.begin;
        auto current_end = current_task.end;
        
        // Partition sequentially using std::partition (much faster than temporary storage)
        for (size_t i = 0; i < current_task.splitters.size(); ++i) {
            const auto& splitter = current_task.splitters[i];
            
            // Partition: elements < splitter go to the left
            auto partition_point = std::partition(current_begin, current_end, 
                [&](const value_type& val) { return comp_(val, splitter); });
            
            current_task.bucket_starts.push_back(partition_point - current_task.begin);
            current_begin = partition_point;
        }
        
        // Last bucket contains remaining elements
        current_task.bucket_starts.push_back(current_end - current_task.begin);
        
        LOG("bucket_starts: ");
        for (auto bucket : current_task.bucket_starts) {
            LOG(bucket, " ");
        }
        LOG("\n");
        LOGW(" -> RECURSE\n");
        
        current_task.state = State::RECURSE;
    }
    
    void handle_recurse() {
        // Extract data we need before popping to avoid expensive task copying
        const auto& current_task = task_stack_.top();
        const auto bucket_starts = std::move(const_cast<Task&>(current_task).bucket_starts);
        const auto begin_iter = current_task.begin;
        task_stack_.pop();
        
        LOGW("RECURSE: Processing ", bucket_starts.size() - 1, " buckets\n");
        
        // Add buckets in reverse order for correct processing
        for(int i = static_cast<int>(bucket_starts.size()) - 1; i > 0; --i) {
            const int bucket_size = bucket_starts[i] - bucket_starts[i - 1];
            LOGW("Bucket ", i-1, " size: ", bucket_size, "\n");
            if(bucket_size > 1) {
                auto bucket_begin = begin_iter + bucket_starts[i - 1];
                auto bucket_end = begin_iter + bucket_starts[i];
                // Use emplace instead of push to avoid unnecessary Task construction
                task_stack_.emplace(bucket_begin, bucket_end, State::SIMPLE_CASES);
            }
        }
    }
};

} // namespace ips4o_sm
