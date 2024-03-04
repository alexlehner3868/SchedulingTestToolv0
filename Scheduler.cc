
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <queue>
#include <limits>

#include "Scheduler.hh"
#include "Test.hh"

using namespace std;
bool all_vehicle_issues_are_last_minute = true;


int count_sites_active(unordered_map<int, vector<BLOCK>> schedule, int num_active_sites){
    int count_active_sites = 0; 

    for (auto& [site_id, tests] : schedule){
        if(tests.size() > 0){
            BLOCK last_testt = tests.back();

            if(last_testt.type == Test){
                count_active_sites++;
            }
        }
       
    }
    return count_active_sites;
}


pair<unordered_map<int, vector<BLOCK>>, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator>> scheduler(unordered_map<int, vector<BLOCK>> schedule, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> queue, float shift_length){

    priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> unscheduled_tests;
    while(!queue.empty()){

        BLOCK current_test = queue.top();
        
        // Which sites can this test go to that wont put it over the shift length (include if there is a temp transition)
        vector<pair<int, float>> site_start; // <site num, start time>
        for(auto possible_site : current_test.possible_sites){

            // check that site is up
            vector<BLOCK> current_site = schedule[possible_site];
            if(current_site.size() == 0){ // Nothing has been scheduled here yet.
                site_start.push_back({possible_site, 0});

            }else{ // Something is already on the schedule 
                BLOCK last_block = current_site.back();

                // check if site is down
                if(last_block.type == Site_Downtime && last_block.end_time == -1 || last_block.unknown_end){
                    continue; // last block was a site downtime and the site is still down 
                }else if(last_block.type == Test){
                    // get conditions of last scheduled test
                    int current_site_end = last_block.end_time;
                    int last_site_temp = last_block.temperature;

                    // calculate what the new test end time would be 
                    float test_duration = current_test.duration;
                    if(last_block.type == Test && last_site_temp != current_test.temperature){
                        test_duration += 30; // add temperature transition
                        if(possible_site == 86 || possible_site == 88) test_duration += 30; // extra 30 min temp transition if on site 88 or 86
                    }

                    if(test_duration + current_site_end <= shift_length){ // check if test fits
                        site_start.push_back({possible_site, current_site_end});
                    }

                }else{
                    // get conditions of last scheduled test
                    int current_site_end = last_block.end_time;
                    float test_duration = current_test.duration;
                    if(test_duration + current_site_end <= shift_length){ // check if test fits
                        site_start.push_back({possible_site, current_site_end});
                    }
                }

            }
            
        }

        // pick earliest start time
        int best_site = -1;
        float best_start = 999999999999999;
        for(auto possible_site : site_start){
            int site_num = possible_site.first;
            float start_time = possible_site.second;
            if(start_time < best_start){
                best_start = start_time;
                best_site = site_num;
            }
        }

        // set start, site and end of test block 
        if(best_site != -1){
            int last_temp_on_site;
            float last_time_on_site = 0;
            if(schedule[best_site].size() != 0){
                last_temp_on_site = schedule[best_site].back().temperature;
                last_time_on_site = schedule[best_site].back().end_time;
                block_type last_type = schedule[best_site].back().type;
                if(last_type == Test && last_temp_on_site != current_test.temperature && current_test.temperature_uniqueness <= 10){
                    BLOCK temperature_transition;
                    temperature_transition.type = Temperature_transition; 
                    temperature_transition.start_time = last_time_on_site;
                    temperature_transition.TR = "Temperature Transition" + to_string(current_test.temperature);
                    temperature_transition.duration = (best_site == 75 || best_site == 76) ? 60 : 30;
                    temperature_transition.end_time = temperature_transition.duration + last_time_on_site;

                    last_time_on_site = temperature_transition.end_time;
                    schedule[best_site].push_back(temperature_transition);  
                    
                }
            }
            current_test.start_time = last_time_on_site;
            current_test.end_time = last_time_on_site + current_test.duration;
            current_test.unknown_end = false;
            schedule[best_site].push_back(current_test);
        }else{
            unscheduled_tests.push(current_test);
        }
        queue.pop();
    }
    return {schedule, unscheduled_tests};
}


vector<pair<string, unordered_map<int, vector<BLOCK>>>> walk_through_changes(unordered_map<int, vector<BLOCK>> initial_schedule, priority_queue<CHANGE, vector<CHANGE>, CHANGE::Comparator> change_log, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> queue, float shift_length, int num_active_sites){
    vector<pair<string, unordered_map<int, vector<BLOCK>>>> schedules;
    cout<<"START"<<endl;
    while(!change_log.empty()){
        string tag_line;
        // Extract Change
        CHANGE current_change = change_log.top();
        float change_time = current_change.time_of_change;
        cout<<"CHANGE"<<endl;
        // Lock all tests before the change. Add unlocked tests to the queue
        for (auto& [site_id, tests] : initial_schedule){
            for(auto& test : tests){
                if(test.start_time > change_time){
                    test.locked = false; // test has not happened yet (it is in the future) 
                    if(test.type == Test) queue.push(test);
                }else if (test.start_time <= change_time){
                    test.locked = true; // test has already started/finished
                }
            }
        }
cout<<"1"<<endl;
        // Remove all unlocked tests 
        for (auto& [site_id, tests] : initial_schedule){
            int num_locked = 0;
            while(num_locked< tests.size() && tests[num_locked].locked){
                num_locked++;
            }
            int num_to_erase = tests.size()-num_locked;
            for(int i = 0; i<num_to_erase; i++){
                tests.pop_back(); // remove the last items on the vector
            }
            if(tests.size() > 0 && tests[tests.size()-1].type == Temperature_transition){
                tests.pop_back();
            }
        }
cout<<"2"<<endl;
        // depending on type of change, add the required delays (might need to remove test if it is locked but wants to be deleted)
        if(current_change.change_type == "SITE DOWN"){
            tag_line = to_string(change_time) + ": " + "Site DOWN --> site_" + to_string(current_change.site_id);
            // What site is going down and when 
            int site_to_go_down = current_change.site_id;
            float downtime_start_time = change_time;

            // Get last test on site 
            if(initial_schedule[site_to_go_down].size()>0){
                BLOCK last_test = initial_schedule[site_to_go_down].back();
                bool last_test_complete = (last_test.end_time <= change_time) ? 1 : 0;
                bool current_test_in_progress = (last_test.start_time > change_time) ? 1 : 0;

                // Cancel the site if it is not done and add it to queue 
                if(last_test.type != Site_Downtime && !last_test_complete || current_test_in_progress){
                    // Remove the last time 
                    //initial_schedule[site_to_go_down].erase(initial_schedule[site_to_go_down].begin()+ initial_schedule[site_to_go_down].size()-1);
                    
                    // Add a delay for time lost for the part of test that ran
                    if(false && last_test.start_time != change_time){
                        BLOCK test_lost;
                        test_lost.start_time = last_test.start_time;
                        test_lost.type = incomplete_test_time_lost_for_site_down;
                        test_lost.end_time = change_time;
                        test_lost.duration = (test_lost.end_time - test_lost.start_time);
                        test_lost.locked = true;
                        test_lost.TR = "Time lost due to site going down mid test";
                        initial_schedule[site_to_go_down].push_back(test_lost);
                    }
                  
                }
            }
      cout<<"3 - site downtime"<<endl;
            // add downtime block with unfinished end time 
            BLOCK site_downtime; 
            site_downtime.start_time = change_time;
            site_downtime.type = Site_Downtime; 
            site_downtime.end_time = -1;
            site_downtime.unknown_end = true;
            site_downtime.TR = "Site Down";
            initial_schedule[site_to_go_down].push_back(site_downtime);
            cout<<"3 - site downtime2"<<endl;
            // See if tests can move to a new site --> remove labor + QC
            int staffed_sites = count_sites_active(initial_schedule, num_active_sites);
            cout<<"3 - site downtime2"<<endl;
            for (auto& [site_id, tests] : initial_schedule){
                if(staffed_sites < num_active_sites){
                    
                    cout<<"in here"<<endl;
                    // Check if site is currently no_labor avail
                    bool no_labor = false;
                    if(tests.size() > 0){
                        BLOCK last_test = tests.back();
                        no_labor = (last_test.type == No_Labor_Available) ? true : false;
                    }else if(tests.size() == 0){
                        no_labor = true;
                    }
                    cout<<"in here - b"<<endl;
                    if(no_labor && tests.size() > 0){
                        // Close no labor block 
                        tests.back().end_time = change_time;
                        tests.back().duration = tests.back().end_time - tests.back().start_time;
                        // add QC to site 
                        BLOCK new_qc;
                        new_qc.type = Site_switch;
                        new_qc.duration = 30;
                        new_qc.start_time = change_time;
                        new_qc.end_time = new_qc.start_time + new_qc.duration;
                        new_qc.TR = "Site Switch";
                        tests.push_back(new_qc);
                        // increment staffed sites 
                        staffed_sites++;
                    }
                    
                }
                   
            }

            cout<<"3b"<<endl;
        }else if(current_change.change_type == "SITE UP"){
            tag_line = to_string(change_time) + ": " + "Site UP --> site_" + to_string(current_change.site_id);
            // get site 
            int site_up = current_change.site_id;

            // Change end time and duration and bool of downtime
            initial_schedule[site_up].back().end_time = change_time;
            initial_schedule[site_up].back().duration =  change_time - initial_schedule[site_up].back().start_time;
            initial_schedule[site_up].back().unknown_end = false;

            // TODO if no labor on that site ---> add a no labor block when it comes back up 
            if(count_sites_active(initial_schedule, num_active_sites) == num_active_sites){
                BLOCK no_labor;
                no_labor.type = No_Labor_Available;
                no_labor.start_time = change_time;
                no_labor.end_time = -1;
                no_labor.unknown_end = true;
                no_labor.TR = "No Labor Avail";
                initial_schedule[site_up].push_back(no_labor);
                
            }
        
        }else if(current_change.change_type == "DELETE"){ // need to look at how much notice we were given and each of the reasons why a test could be deleted
            tag_line = to_string(change_time) + ": " + "Removed " +current_change.TR + " from schedule and rescheduled --> " + current_change.cancellation_reason;
            for (auto& [site_id, tests] : initial_schedule){
                int i = 0;
                for(i = 0; i < tests.size(); i++){
                    if(tests[i].TR == current_change.TR){
                        break;
                    }
                }
                if(i != tests.size()){
                    
                    if(current_change.cancellation_reason == "Vehicle Issues"){
                        float test_start = tests[i].start_time;
                        float change_time = current_change.time_of_change;
                        if(!all_vehicle_issues_are_last_minute && change_time < test_start){
                            tests.erase(tests.begin()+i);
                        }else{
                            
                            BLOCK delay;
                            delay.type = Lost_time_last_minute_vehicle_issue;
                            delay.duration = 30;
                            delay.locked = true;
                            delay.start_time = test_start;
                            delay.end_time = test_start + delay.duration;
                            delay.temperature = 75;
                            delay.TR = "Time lost to last minute vehicle issues";
                            tests.erase(tests.begin()+i);
                            tests.push_back(delay);
                        }
                        
                        
                    }else if(current_change.cancellation_reason == "RE Change" || current_change.cancellation_reason == "Requester Cancelled "){
                        float test_start = tests[i].start_time;
                        float change_time = current_change.time_of_change;
                        if(change_time < test_start){
                            tests.erase(tests.begin()+i);
                        }else{
                            
                            BLOCK delay;
                            delay.type = Lost_time_last_minute_RE_change;
                            delay.duration = 15;
                            delay.locked = true;
                            delay.start_time = test_start;
                            delay.end_time = test_start + delay.duration;
                            delay.temperature = 75;
                            delay.TR = "Time lost to last minute RE change";
                            tests.erase(tests.begin()+i);
                            tests.push_back(delay);
                        }
                    }else if(current_change.cancellation_reason == "Unknown"){
                        float test_start = tests[i].start_time;
                        float change_time = current_change.time_of_change;
                        if(change_time < test_start){
                            tests.erase(tests.begin()+i);
                        }else{
                            
                            BLOCK delay;
                            delay.type = unknown_lost_time;
                            delay.duration = 15;
                            delay.locked = true;
                            delay.start_time = test_start;
                            delay.end_time = test_start + delay.duration;
                            delay.temperature = 75;
                            delay.TR = "Time lost to last minute test cancellation (reason unknown)";
                            tests.erase(tests.begin()+i);
                            tests.push_back(delay);
                        }
                    }else if(current_change.cancellation_reason == "Site Issue"){
                            BLOCK delay;
                            delay.type = Lost_time_Site_Issue;
                            delay.duration = 45;
                            delay.locked = true;
                            delay.start_time = tests[i].start_time;
                            delay.end_time = tests[i].start_time + delay.duration;
                            delay.temperature = 75;
                            delay.TR = "Time lost to site issue";
                            tests.erase(tests.begin()+i);
                            tests.push_back(delay);
                    }else{
                        cout<<" Unrecognized cancellation reason"<<endl;
                    }
                    
                    
                }else{
                    priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> temp_queue;
                    while(!queue.empty()){
                        BLOCK q = queue.top();
                        if(q.TR != current_change.TR){
                            temp_queue.push(q);
                        }
                        queue.pop();
                    }
                    queue = temp_queue;
                }
            }
        }else if(current_change.change_type == "ADDED"){
            tag_line = to_string(change_time) + ": " + "Added test " + current_change.added_test.TR + " to the queue and rescheduled";
            queue.push(current_change.added_test);
        }
            cout<<"4"<<endl;
        // re-fill the schedule 
        pair<unordered_map<int, vector<BLOCK>>, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator>> updated_info = scheduler(initial_schedule, queue, shift_length);
        initial_schedule = updated_info.first;
        queue = updated_info.second;
        cout<<"5"<<endl;
        // add the schedule and a string tag line of what happened to schedules 
        schedules.push_back({tag_line, initial_schedule});
        change_log.pop();
    }

    return schedules;
}
