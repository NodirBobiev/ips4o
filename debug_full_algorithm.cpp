#include "ips4o_state_machine.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <random>

int main() {
    // Create a random array that will definitely trigger full algorithm
    std::vector<int> test_vec;
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 100);
    
    for (int i = 0; i < 300; ++i) {
        test_vec.push_back(dis(gen));
    }
    
    std::cout << "Array size: " << test_vec.size() << std::endl;
    std::cout << "First 20 elements before: ";
    for (int i = 0; i < 20; ++i) std::cout << test_vec[i] << " ";
    std::cout << std::endl;
    
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(test_vec.begin(), test_vec.end());
    
    int step_count = 0;
    while (sm.get_state() != ips4o_sm::State::DONE && step_count < 1000) {
        auto old_state = sm.get_state();
        sm.step();
        auto new_state = sm.get_state();
        
        if (step_count < 20 || old_state != new_state) {
            std::cout << "Step " << step_count << ": " << static_cast<int>(old_state) 
                      << " -> " << static_cast<int>(new_state) << std::endl;
        }
        step_count++;
    }
    
    std::cout << "Final state: " << static_cast<int>(sm.get_state()) << std::endl;
    std::cout << "Steps taken: " << step_count << std::endl;
    
    std::cout << "First 20 elements after: ";
    for (int i = 0; i < 20; ++i) std::cout << test_vec[i] << " ";
    std::cout << std::endl;
    
    bool is_sorted = std::is_sorted(test_vec.begin(), test_vec.end());
    std::cout << "Is sorted: " << (is_sorted ? "YES" : "NO") << std::endl;
    
    if (!is_sorted) {
        std::cout << "First unsorted position: ";
        for (int i = 0; i < test_vec.size() - 1; ++i) {
            if (test_vec[i] > test_vec[i + 1]) {
                std::cout << "Position " << i << ": " << test_vec[i] << " > " << test_vec[i + 1] << std::endl;
                break;
            }
        }
    }
    
    return 0;
}
