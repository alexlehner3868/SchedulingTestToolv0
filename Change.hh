#ifndef CHANGE_H
#define CHANGE_H

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <queue>

#include "Test.hh"


using namespace std;



class CHANGE {
    public:
 
    string change_type;
    float time_of_change;

    // If adding test
    BLOCK added_test;

    // If deleting test
    string cancellation_reason;
    string TR;

    // If Site up or down 
    int site_id; 
    
    // empty block 
    CHANGE() {}
    
    struct Comparator {
        bool operator()(const CHANGE& lhs, const CHANGE& rhs) const {
            return lhs.time_of_change > rhs.time_of_change;
        }
    };
   
};

#endif