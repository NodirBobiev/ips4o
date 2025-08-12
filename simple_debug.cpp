#include "ips4o_state_machine.hpp"
#include <vector>
#include <iostream>
#include <algorithm>

int main() {
    // Test with a very simple array that should trigger sampling
    std::vector<int> test_vec = {5, 1, 4, 2, 8, 3, 7, 6, 9, 0, 
                                 15, 11, 14, 12, 18, 13, 17, 16, 19, 10};
    
    std::cout << "Before: ";
    for (int x : test_vec) std::cout << x << " ";
    std::cout << std::endl;
    
    // Force it to use full algorithm by making it larger than base case
    while (test_vec.size() < 300) {
        test_vec.push_back(test_vec.size());
    }
    
    std::cout << "Array size: " << test_vec.size() << std::endl;
    
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(test_vec.begin(), test_vec.end());
    
    // Step through manually to see exactly what happens
    int step = 0;
    while (sm.get_state() != ips4o_sm::State::DONE && step < 50) {
        auto old_state = sm.get_state();
        std::cout << "Step " << step << ": State " << static_cast<int>(old_state);
        
        sm.step();
        
        auto new_state = sm.get_state();
        std::cout << " -> " << static_cast<int>(new_state) << std::endl;
        
        // Show first few elements after each step
        if (step < 10) {
            std::cout << "  First 10: ";
            for (int i = 0; i < 10 && i < test_vec.size(); ++i) {
                std::cout << test_vec[i] << " ";
            }
            std::cout << std::endl;
        }
        
        step++;
    }
    
    std::cout << "\nFinal result: ";
    for (int i = 0; i < 20 && i < test_vec.size(); ++i) {
        std::cout << test_vec[i] << " ";
    }
    std::cout << std::endl;
    
    bool sorted = std::is_sorted(test_vec.begin(), test_vec.end());
    std::cout << "Is sorted: " << (sorted ? "YES" : "NO") << std::endl;
    
    return 0;
}
