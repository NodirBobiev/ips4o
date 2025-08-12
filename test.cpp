#include "ips4o_state_machine.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>

int main() {
    // Create a random array that will definitely trigger full algorithm
    std::vector<int> test_vec;
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 10000);
    
    const int N = 1000000;

    for (int i = 0; i < N; ++i) {
        test_vec.push_back(dis(gen));
    }
    
    std::cout << "Array size: " << test_vec.size() << std::endl;
    // for (int i = 0; i < N; ++i) std::cout << test_vec[i] << " ";
    // std::cout << std::endl;
    
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(test_vec.begin(), test_vec.end());
    
    
    auto start = std::chrono::high_resolution_clock::now();
    sm.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Time Elapsed: " << elapsed.count() / 1000.0 << " ms\n";

    // std::cout << "Array after sorting: ";
    // for (int i = 0; i < N; ++i) std::cout << test_vec[i] << " ";
    // std::cout << std::endl;

    assert(std::is_sorted(test_vec.begin(), test_vec.end()) && "Array is not sorted");
    
    std::cout << "Array is sorted\n";

    return 0;
}
