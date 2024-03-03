#ifndef TEST_H
#define TEST_H

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <queue>



using namespace std;



enum block_type {
    Test = 0, // this is a testing block
    Site_Downtime = 1, // this is a site downtime 
    Lunch = 2, 
    Temperature_transition = 3,
    QC = 4, 
    No_Labor_Available = 5, // site is down becuase we dont have enough people to staff it 
    Lost_time_Site_Issue = 6, // In addition to the actual site downtime, there is time lost due to having to get/recive and setup car
    Site_switch = 7, // if site goes down and we move to a dif site, we need to redo QCs + return current vehicle so there is a delay 
    Lost_time_No_Re = 8, // RE doesnt show up --> delay
    Lost_time_last_minute_RE_change = 9, // RE changed a test very last minute (after or less than 30 minutes notice)
    Lost_time_last_minute_vehicle_issue = 10, // vehicle had an issue last minute (when on that vehicle)
    planned_downtimes = 11,
    unknown_lost_time = 12,
    incomplete_test_time_lost_for_site_down = 13
};

class BLOCK {
    public:

    block_type type; 
    
    float duration;

    float start_time;
    float end_time;
    bool unknown_end; //used for site_downs when we dont know how long it will be down, until it is no longer down

    bool scheduled;
    bool complete;
    bool in_progress; 
    bool locked; // can this block be moved? 

    // variables for if it is a test
    string TR;
    unordered_set<int> possible_sites; 
    int temperature;
    int temperature_uniqueness; 
    int num_possible_sites;
    int priority_level; // 0 is highest

    // empty block 
    BLOCK() {}
    
    // Test 
    BLOCK(int p_level, float dur, int temp, unordered_set<int> p_sites){
        type = Test;
        
        priority_level = p_level;
        duration = dur;
        temperature = temp;
        possible_sites.clear();
        possible_sites = p_sites;
    }

    BLOCK(block_type t){
        type = t;
    }
 

    void scheduled_print(){
        cout<<TR<<" : ("<<start_time<<" -> "<<end_time<<")"<<endl;
    }

    struct Comparator {
        bool operator()(const BLOCK& lhs, const BLOCK& rhs) const {
            // Order by priority level (lower numbers first)
            if (lhs.priority_level != rhs.priority_level) {
                return lhs.priority_level > rhs.priority_level;
            }

            // Order by number of possible sites (lower numbers first)
            if (lhs.num_possible_sites != rhs.num_possible_sites) {
                return lhs.num_possible_sites > rhs.num_possible_sites;
            }

            
            // Order by temperature uniqueness (higher numbers first)
            if (lhs.temperature_uniqueness != rhs.temperature_uniqueness) {
                return lhs.temperature_uniqueness < rhs.temperature_uniqueness;
            }

            // Order by duration (higher duration first)
            return lhs.duration < rhs.duration;;
        }
    };
   
};

#endif