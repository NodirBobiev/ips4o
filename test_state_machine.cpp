#include "ips4o_state_machine.hpp"
#include <vector>
#include <iostream>
#include <algorithm>

int main() {
    // Test 1: Empty array
    {
        std::vector<int> empty_vec;
        ips4o_sm::StateMachine<std::vector<int>::iterator> sm(empty_vec.begin(), empty_vec.end());
        
        std::cout << "Test 1 - Empty array:\n";
        std::cout << "Initial state: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // INIT -> SIMPLE_CASES
        std::cout << "After INIT: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // SIMPLE_CASES -> DONE
        std::cout << "After SIMPLE_CASES: " << static_cast<int>(sm.get_state()) << std::endl;
        std::cout << "Expected: 7 (DONE)\n\n";
    }
    
    // Test 2: Already sorted array
    {
        std::vector<int> sorted_vec = {1, 2, 3, 4, 5};
        ips4o_sm::StateMachine<std::vector<int>::iterator> sm(sorted_vec.begin(), sorted_vec.end());
        
        std::cout << "Test 2 - Already sorted array:\n";
        std::cout << "Initial state: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // INIT -> SIMPLE_CASES
        std::cout << "After INIT: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // SIMPLE_CASES -> DONE
        std::cout << "After SIMPLE_CASES: " << static_cast<int>(sm.get_state()) << std::endl;
        std::cout << "Expected: 7 (DONE)\n\n";
    }
    
    // Test 3: Reverse sorted array
    {
        std::vector<int> reverse_vec = {5, 4, 3, 2, 1};
        ips4o_sm::StateMachine<std::vector<int>::iterator> sm(reverse_vec.begin(), reverse_vec.end());
        
        std::cout << "Test 3 - Reverse sorted array:\n";
        std::cout << "Initial state: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // INIT -> SIMPLE_CASES
        std::cout << "After INIT: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // SIMPLE_CASES -> DONE (should reverse)
        std::cout << "After SIMPLE_CASES: " << static_cast<int>(sm.get_state()) << std::endl;
        std::cout << "Expected: 7 (DONE)\n";
        std::cout << "Array after: ";
        for (int x : reverse_vec) std::cout << x << " ";
        std::cout << "\nExpected: 1 2 3 4 5\n\n";
    }
    
    // Test 4: Small array (base case)
    {
        std::vector<int> small_vec = {3, 1, 4, 1, 5, 9, 2, 6};
        ips4o_sm::StateMachine<std::vector<int>::iterator> sm(small_vec.begin(), small_vec.end());
        
        std::cout << "Test 4 - Small array (base case):\n";
        std::cout << "Initial state: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // INIT -> SIMPLE_CASES
        std::cout << "After INIT: " << static_cast<int>(sm.get_state()) << std::endl;
        
        sm.step(); // SIMPLE_CASES -> BASE_CASE
        std::cout << "After SIMPLE_CASES: " << static_cast<int>(sm.get_state()) << std::endl;
        std::cout << "Expected: 2 (BASE_CASE)\n";
        
        sm.step(); // BASE_CASE -> DONE (should sort)
        std::cout << "After BASE_CASE: " << static_cast<int>(sm.get_state()) << std::endl;
        std::cout << "Expected: 7 (DONE)\n";
        std::cout << "Array after: ";
        for (int x : small_vec) std::cout << x << " ";
        std::cout << "\nExpected: 1 1 2 3 4 5 6 9\n\n";
    }
    
    return 0;
}
