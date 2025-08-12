#include "ips4o_state_machine.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <random>
#include <chrono>

void assert_sorted(const std::vector<int>& vec, const std::string& test_name, double elapsed_ms = -1) {
    bool is_sorted = std::is_sorted(vec.begin(), vec.end());
    if (!is_sorted) {
        std::cout << "FAILED: " << test_name << " - Array is not sorted!\n";
        std::cout << "Array: ";
        for (int x : vec) std::cout << x << " ";
        std::cout << std::endl;
        assert(false);
    } else {
        std::cout << "PASSED: " << test_name;
        if (elapsed_ms >= 0) {
            std::cout << " (" << elapsed_ms << " ms)";
        }
        std::cout << std::endl;
    }
}

void test_empty_array() {
    std::vector<int> empty_vec;
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(empty_vec.begin(), empty_vec.end());
    
    auto start = std::chrono::high_resolution_clock::now();
    sm.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    assert(sm.is_done());
    assert_sorted(empty_vec, "Empty array", elapsed.count() / 1000.0);
}

void test_already_sorted() {
    std::vector<int> sorted_vec = {1, 2, 3, 4, 5};
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(sorted_vec.begin(), sorted_vec.end());
    
    auto start = std::chrono::high_resolution_clock::now();
    sm.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    assert(sm.is_done());
    assert_sorted(sorted_vec, "Already sorted array", elapsed.count() / 1000.0);
}

void test_reverse_sorted() {
    std::vector<int> reverse_vec = {5, 4, 3, 2, 1};
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(reverse_vec.begin(), reverse_vec.end());
    
    auto start = std::chrono::high_resolution_clock::now();
    sm.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    assert(sm.is_done());
    assert_sorted(reverse_vec, "Reverse sorted array", elapsed.count() / 1000.0);
}

void test_small_array() {
    std::vector<int> small_vec = {3, 1, 4, 1, 5, 9, 2, 6};
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(small_vec.begin(), small_vec.end());
    
    auto start = std::chrono::high_resolution_clock::now();
    sm.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    assert(sm.is_done());
    assert_sorted(small_vec, "Small array (base case)", elapsed.count() / 1000.0);
}

void test_medium_array() {
    std::vector<int> medium_vec;
    for (int i = 100; i >= 1; --i) {
        medium_vec.push_back(i);
    }
    
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(medium_vec.begin(), medium_vec.end());
    auto start = std::chrono::high_resolution_clock::now();
    sm.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    assert(sm.is_done());
    assert_sorted(medium_vec, "Medium array (100 elements)", elapsed.count() / 1000.0);
}

void test_large_array() {
    std::vector<int> large_vec;
    std::mt19937 gen(42); // Fixed seed for reproducible results
    std::uniform_int_distribution<> dis(1, 1000);
    
    for (int i = 0; i < 500; ++i) {
        large_vec.push_back(dis(gen));
    }
    
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(large_vec.begin(), large_vec.end());
    auto start = std::chrono::high_resolution_clock::now();
    sm.run();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    assert(sm.is_done());
    assert_sorted(large_vec, "Large random array (500 elements)", elapsed.count() / 1000.0);
}

void test_duplicates() {
    std::vector<int> dup_vec = {5, 3, 5, 1, 3, 5, 1, 3, 5};
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(dup_vec.begin(), dup_vec.end());
    
    sm.run();
    assert(sm.get_state() == ips4o_sm::State::DONE);
    assert_sorted(dup_vec, "Array with duplicates");
}

void test_single_element() {
    std::vector<int> single_vec = {42};
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(single_vec.begin(), single_vec.end());
    
    sm.run();
    assert(sm.get_state() == ips4o_sm::State::DONE);
    assert_sorted(single_vec, "Single element array");
}

void test_two_elements() {
    std::vector<int> two_vec = {2, 1};
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(two_vec.begin(), two_vec.end());
    
    sm.run();
    assert(sm.get_state() == ips4o_sm::State::DONE);
    assert_sorted(two_vec, "Two element array");
}

void test_all_equal() {
    std::vector<int> equal_vec(50, 7);
    ips4o_sm::StateMachine<std::vector<int>::iterator> sm(equal_vec.begin(), equal_vec.end());
    
    sm.run();
    assert(sm.get_state() == ips4o_sm::State::DONE);
    assert_sorted(equal_vec, "All equal elements");
}

int main() {
    std::cout << "Running IPS4o State Machine Tests...\n\n";
    
    try {
        test_empty_array();
        test_single_element();
        test_two_elements();
        test_already_sorted();
        test_reverse_sorted();
        test_small_array();
        test_duplicates();
        test_all_equal();
        test_medium_array();
        test_large_array();
        
        std::cout << "\n✅ All tests passed!\n";
    } catch (const std::exception& e) {
        std::cout << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
