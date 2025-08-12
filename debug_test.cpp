#include "ips4o_state_machine.hpp"
#include <vector>
#include <iostream>
#include <algorithm>

int main() {
    // Test array larger than base case threshold (256) to trigger full algorithm
    std::vector<int> test_vec;
    for (int i = 300; i >= 1; --i) {
        test_vec.push_back(i);
    }
    
    std::cout << "Before sorting: ";
    for (int x : test_vec) std::cout << x << " ";
    std::cout << std::endl;
    
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(test_vec.begin(), test_vec.end());
    
    std::cout << "Initial state: " << static_cast<int>(sm.get_state()) << std::endl;
    
    int step_count = 0;
    while (sm.get_state() != ips4o_sm::State::DONE && step_count < 100) {
        auto old_state = sm.get_state();
        sm.step();
        auto new_state = sm.get_state();
        std::cout << "Step " << step_count << ": " << static_cast<int>(old_state) 
                  << " -> " << static_cast<int>(new_state) << std::endl;
        step_count++;
    }
    
    std::cout << "Final state: " << static_cast<int>(sm.get_state()) << std::endl;
    std::cout << "Steps taken: " << step_count << std::endl;
    
    std::cout << "After sorting: ";
    for (int x : test_vec) std::cout << x << " ";
    std::cout << std::endl;
    
    bool is_sorted = std::is_sorted(test_vec.begin(), test_vec.end());
    std::cout << "Is sorted: " << (is_sorted ? "YES" : "NO") << std::endl;
    
    return 0;
}
