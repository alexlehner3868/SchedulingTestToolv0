#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <queue>

#include "Test.hh"
#include "Change.hh"
using namespace std;


pair<unordered_map<int, vector<BLOCK>>, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator>> scheduler(unordered_map<int, vector<BLOCK>> schedule, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> queue, float shift_length);


void lock_tests(unordered_map<int, vector<BLOCK>> schedule, float time);


vector<pair<string, unordered_map<int, vector<BLOCK>>>>  walk_through_changes(unordered_map<int, vector<BLOCK>> initial_schedule, priority_queue<CHANGE, vector<CHANGE>, CHANGE::Comparator> change_log, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> queue, float shift_length, int num_active_sites);
#endif