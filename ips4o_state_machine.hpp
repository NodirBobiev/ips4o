#pragma once

#include <iterator>
#include <stack>
#include <functional>
#include <random>
#include <vector>
#include <fstream>
#include "ips4o/config.hpp"
#include <iostream>

using namespace std;

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
    PARTITION 
};

std::vector<std::string> state_names = {
    "SIMPLE_CASES",
    "BASE_CASE",
    "SAMPLE_SELECT",
    "SAMPLE_SORTED",
    "PARTITION"
};


template <class Iterator, class Comparator = std::less<>, class Config = ips4o::Config<>>
class StateMachine {
public:
    using iterator = Iterator;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using difference_type = typename std::iterator_traits<Iterator>::difference_type;
    using comparator = Comparator;
    
    struct Task {
        std::vector<value_type> splitters;  
        iterator begin;                     
        iterator end;                       
        difference_type num_samples;        
        difference_type step;               
        int num_buckets;                    
        State state;                        

        Task(iterator b, iterator e, State s = State::SIMPLE_CASES) 
            : splitters(), begin(b), end(e), num_samples(0), step(0), num_buckets(0), state(s) {}
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
            case State::PARTITION:
                handle_partition();
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
        const int log_buckets = Config::logBuckets(n);
        context_task.num_buckets = 1 << log_buckets;
        context_task.step = std::max<difference_type>(1, Config::oversamplingFactor(n));
        context_task.num_samples = std::min(context_task.step * context_task.num_buckets - 1, n / 2);

        LOGW("log_buckets:", log_buckets, "num_buckets:", context_task.num_buckets, "step:", context_task.step, "num_samples:", context_task.num_samples, "\n");

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
        context.splitters.reserve(context.num_buckets - 1);  // Reserve capacity to avoid reallocations
        
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
        
        context.state = State::PARTITION;
        LOG("splitters: ");
        for (auto splitter : context.splitters) {
            LOG(splitter, " ");
        }
        LOG("\n");
        LOGW(" -> PARTITION\n");
    }
    
    void handle_partition() {
        // Extract data we need before popping to avoid expensive task copying
        const auto& current_task = task_stack_.top();
        const auto splitters = std::move(const_cast<Task&>(current_task).splitters);  // Move splitters (no copy!)
        auto current_begin = current_task.begin;
        auto current_end = current_task.end;
        task_stack_.pop();  // Pop current task immediately
        
        LOGW("PARTITION: Single-pass partitioning ", current_end - current_begin, " elements with ", splitters.size(), " splitters\n");
        
        // Sequential partitioning with immediate task creation - no intermediate storage!
        for (size_t i = 0; i < splitters.size(); ++i) {
            const auto& splitter = splitters[i];
            
            // Partition: elements < splitter go to the left
            auto partition_point = std::partition(current_begin, current_end, 
                [&](const value_type& val) { return comp_(val, splitter); });
            
            // Immediately push the bucket if it has >1 element
            const auto bucket_size = partition_point - current_begin;
            LOGW("Bucket ", i, " size: ", bucket_size, "\n");
            if(bucket_size > 1) {
                task_stack_.emplace(current_begin, partition_point, State::SIMPLE_CASES);
            }
            
            current_begin = partition_point;  // Move to next range
        }
        
        // Handle final bucket (elements >= last splitter)
        const auto final_bucket_size = current_end - current_begin;
        LOGW("Final bucket size: ", final_bucket_size, "\n");
        if(final_bucket_size > 1) {
            task_stack_.emplace(current_begin, current_end, State::SIMPLE_CASES);
        }
    }
};

} // namespace ips4o_sm
