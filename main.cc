
#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <functional>

#include "Test.hh"
#include "Scheduler.hh"
#include "Change.hh"

using namespace std;

/*
0. Define Data structures  -- DONE 
    - Block    -- DONE 
    - Priority queue for unschedule -- DONE 
    -- schedule -- DONE 
    -- Define Action List (by start time) -- DONE 

1. Scheduling 
    - basic scheduling functions [3] -- DONE 
    - Add in temperature transitions [4] -- DONE 
        - Adjust for longer temp transition on that one certain site [4b] -- DONE 
    - Add in lunch periods (first version + increase EOS buffer to account for lunch breaks) [6] -- DONE (manual)
    - Keep EOS buffer [5] -- DONE 
    -
2. TR prioritizer -- DONE 

3. Excel Parser --> get data
    - Get Schedule 
        - Manually Add in temperature trans blocks, QCs and lunches to initial schedule data [2] DONE 
        - parse schedule [3] DONE 
    - Get change log [7] -- DONE 
    - Get test queue (USE PM tests but make the queue longer by adding AM tests doubled) [9]
    
4. Walk through change log  [8]
    - Function to set blocks as in_progress, complete, scheduled (to know what can be changed) --> lock tests that can't be moved 
    - Function to remove all blocks that are not in_progress or complete from the schedule (and not a site downtime)
        - Identify which case we are in and Handle each case seperately 
            - SWAP 
            - RE Test deleted --> last minute, plenty of time, last minute EOS
            - Test added --> add to queue of possible tests to run (assume: low priority since we dont have priority of add-in tests)
            - Site down --> decide if and when to move to a new site (see if data gives indication of whether or not DT is fast)
            - Vehicle issue --> check if last minute or not and react accordingly 
            - RE not present 
            - Site comes back up
        - add required delays, remove all unlocked tests, re-prioritize queue, re-schedule 
5. Output each step to file  [10]
6. Output fill rates and time lost due to different reasons [11]

 
*/

unordered_set<int> extractNumbers(const std::string& input, unordered_set<int> available_sites) {
    std::unordered_set<int> numbers;
    std::istringstream iss(input);

    // Read each comma-separated value
    while (!iss.eof()) {
        // Skip non-numeric characters
        while (!isdigit(iss.peek()) && iss.peek() != EOF) {
            iss.ignore();
        }

        // Extract the next number
        int number;
        iss >> number;


        // Add the number to the vector
        if(available_sites.find(number) != available_sites.end()){
            numbers.insert(number);
        }
        

        // Skip the comma if present
        if (iss.peek() == ',') {
            iss.ignore();
        }
    }
    
    return numbers;
}

void read_schedule_csv(std::string filename, unordered_map<int, vector<BLOCK>>& schedule, priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator>& queue, unordered_set<int> available_sites) { //function from chat gpt
    
    std::ifstream myFile(filename);
    
    if (!myFile.is_open())
        throw std::runtime_error("Could not open file");

    std::string line;
    
    int first_row = true;
    while (std::getline(myFile, line)) {
        std::vector<std::string> row;
        std::istringstream ss(line);
        std::string value;

        int col = 0;
        int temp;
        int temp_uniqueness;
        string possible_sites = "";
        bool multiple_possible_sites = false;
        unordered_set<int> set_of_sites;
        int num_possible_sites;
        int priority_level;
        float duration;
        string TR;
        int assigned_site = -1;

        float start = -1;
        float end = -1;

        while (!first_row && getline(ss, value, ',')) {
            //cout<<col<<" : "<<value<<endl;
            if(col == 0){
                temp = stoi(value);
            }else if(col == 1){
                temp_uniqueness = stoi(value);
            }else if(col == 2){
                TR = value;
            }else if(col == 3){
                // do nothing --> overrided priority column 
            }else if(col == 4){
                priority_level = stoi(value);
            }else if(col == 5){
                num_possible_sites = stoi(value);
            }else if(col == 6){
                duration = stof(value);
            }else if(col == 7){
                possible_sites += value;
                if(possible_sites[0] == '"') multiple_possible_sites = true;
                if(value.back() == '"') multiple_possible_sites = false;

                if(multiple_possible_sites) col--;
            }else if(col == 8){
                if(value != "---" && value != "—") {
                    unordered_set<int> possible_sites = extractNumbers(value, available_sites);
                    if(possible_sites.size() > 0) assigned_site = *begin(possible_sites);
                }
            }else if(col == 9){
                if(value != "-1" && value != "—"){
                    start = stof(value);
                }
            }else if(col == 10){
                //cout << "value is " << value << " " << (value == "---") << endl;
                if (value != "-1" && value != "—") {
                    end = stof(value);
                }
            }
            
            col++;
        }

        // Save the new data into its respective data fields
        if(!first_row){
            BLOCK b;
            // Check if the pulled block is a test or a downtime 
            if(TR.compare(0, 4, "Site") == 0){ // Downtime 
                b.type = Site_Downtime; 
                b.duration = duration;
                b.temperature = temp;
                b.temperature_uniqueness = temp_uniqueness;
                b.priority_level = priority_level;
                b.TR = TR;
                b.start_time = start;
                b.end_time = end;
                b.possible_sites  = extractNumbers(possible_sites, available_sites);
            }else{ // Test 
                b.type = Test; 
                b.duration = duration;
                b.temperature = temp;
                b.temperature_uniqueness = temp_uniqueness;
                b.priority_level = priority_level;
                b.TR = TR;
                b.start_time = start;
                b.end_time = end;
                b.num_possible_sites = num_possible_sites;
                b.possible_sites  = extractNumbers(possible_sites, available_sites);
            }
            
            if(assigned_site == -1){
                queue.push(b);
            }else{
                schedule[assigned_site].push_back(b);
            }
        }

        first_row = false;

        
    }

    myFile.close();

    return;
}


void read_change_log_csv(string filename, priority_queue<CHANGE, vector<CHANGE>, CHANGE::Comparator>& change_log, unordered_set<int> available_sites){

    std::ifstream myFile(filename);
    
    if (!myFile.is_open())
        throw std::runtime_error("Could not open file");

    std::string line;
    
    int first_row = true;
    while (std::getline(myFile, line)) {
        std::vector<std::string> row;
        std::istringstream ss(line);
        std::string value;

        // define variables here 
        int col = 0;
        string TRNo_or_site; 
        string action; 
        string possible_sites;
        string removal_category;
        bool multiple_possible_sites = false;
        float start_time; 
        float duration;
        float downtime_duration;
        while (!first_row && getline(ss, value, ',')) {
            //cout<<col<<" : "<<value<<endl;
            if(col == 0){
                TRNo_or_site =  value;
            }else if(col == 1){
               action = value;
            }else if(col == 2){
                possible_sites += value;
                if(possible_sites[0] == '"') multiple_possible_sites = true;
                if(value.back() == '"') multiple_possible_sites = false;

                if(multiple_possible_sites) col--;
            }else if(col == 3){
                removal_category = value;
            }else if(col == 4){
                //cout<<"start "<<value<<endl;
                start_time = stof(value);
            }else if(col ==5){
                //cout<<"duration "<<value<<endl;
                duration = stof(value);
            }
            
            col++;
        }

        // Save the new data into its respective data fields
        if(!first_row){
            CHANGE new_change;
            new_change.change_type = action;
            new_change.time_of_change = start_time;
            new_change.duration = duration;
            if(action == "ADDED"){
                BLOCK new_test; 
                new_test.duration = duration;
                new_test.possible_sites = extractNumbers(possible_sites, available_sites);
                new_test.TR = TRNo_or_site;
                new_test.temperature = 75; // Assuming new test is at ambient temp 
                new_test.priority_level = 1; // Assume test is added because it is a high priority
                new_test.temperature_uniqueness = 135; // assume it is the common temp
                new_test.num_possible_sites = new_test.possible_sites.size();
                new_change.added_test = new_test;
            }else if(action == "DELETE"){
                new_change.cancellation_reason = removal_category;
                new_change.TR = TRNo_or_site;
                
            }else if(action == "SITE UP" || action == "SITE DOWN"){
                new_change.site_id = *begin(extractNumbers(TRNo_or_site, available_sites)); // get the site number that is going up/down
            }else{
                cout<<"Unrecognized action provided ---> "<<action<<endl;
            }
            if(removal_category != "Scheduling Error") change_log.push(new_change);
        }

        first_row = false;
        
    }

    myFile.close();

    return;

}

void print_schedule(unordered_map<int, vector<BLOCK>> schedule){
    for (auto& [site_id, tests] : schedule){
        cout<<"Site_"<<site_id<<endl;
        for(auto& test : tests){
            test.scheduled_print();
        }
        cout<<endl;
    } 
    cout<<endl;
}

int main()
{

    int num_active_sites = 18; // One shift at a time 

    float EOS_buffer =  0.02*8.5*60; // time to keep back at EOS to account for delays 
    float lunch_length = 30;
    float initial_QCs = 30; // hold 30 min off available time for initial QCs 

    float shift_duration = 8.5*60; 
    float available_time = shift_duration - initial_QCs - lunch_length - EOS_buffer; 

    // Create empty schedule for each site 
    unordered_set<int> sites = {30, 58, 78, 83, 84, 22, 82, 74, 72, 76, 86, 88, 75, 26, 87, 73, 32, 80, 40, 34, 89, 81, 70, 24, 85};
    unordered_map<int, vector<BLOCK>> schedule; 
    for(auto site: sites){
        schedule[site] = vector<BLOCK>();
    }

    
    // unscheduled testing queue 
    priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> queue; 
    cout<<"Reading initial schedule data ...";
    std::string filename = "/Users/alex/Desktop/Feb15AMList.csv";  // Change this to your CSV file path

    read_schedule_csv(filename, schedule, queue, sites);
    cout<<" complete"<<endl;
    
    // Pull List of changes to the schedule 
    std::string filename_changelog = "/Users/alex/Desktop/Feb15AMChanges.csv";  // Change this to your CSV file path
    priority_queue<CHANGE, vector<CHANGE>, CHANGE::Comparator> change_log; 

    cout<<"Reading change log data ...";
    read_change_log_csv(filename_changelog, change_log, sites);
    cout<<" complete"<<endl;

    cout<<"Walking through change log ...";
    vector<pair<string, unordered_map<int, vector<BLOCK>>>> schedules = walk_through_changes(schedule, change_log, queue, available_time, num_active_sites);
    cout<<" complete"<<endl;

    ofstream myfile;
    myfile.open ("FEB15_AM_schedules_LIMITSITES.txt");
 
    for(auto schedule : schedules){
        int filled_hours = 0;
        if(schedule.first != ""){
            myfile<<endl;
            myfile<<"------------------------------"<<endl;
            myfile<< schedule.first<<endl;
            for (auto& [site_id, tests] : schedule.second){
               // myfile<<"SITE_"<<site_id<<endl;
                for(auto& test : tests){
                    myfile <<test.TR<<", "<<site_id <<", "<< test.start_time<<","<<test.end_time<<endl;
                    if(test.type == Test) filled_hours+=test.duration;
                }
            }
            myfile <<" hours filled: "<< filled_hours/60<<endl;
        }
    }
    myfile.close();
    

    // PM 
    num_active_sites = 18; // 16 active sites in the PM 

    unordered_map<int, vector<BLOCK>> PM_schedule; 
    for(auto site: sites){
        PM_schedule[site] = vector<BLOCK>();
    }

    // unscheduled testing queue 
    priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> PM_queue; 
    cout<<"Reading initial schedule data ...";
    std::string PM_filename = "/Users/alex/Desktop/Feb15PM.csv";  // Change this to your CSV file path

    read_schedule_csv(PM_filename, PM_schedule, PM_queue, sites);
    cout<<" complete"<<endl;
    
    // Pull List of changes to the schedule 
    std::string PM_filename_changelog = "/Users/alex/Desktop/Feb15PMChanges.csv";  // Change this to your CSV file path
    priority_queue<CHANGE, vector<CHANGE>, CHANGE::Comparator> PM_change_log; 

    cout<<"Reading change log data ...";
    read_change_log_csv(PM_filename_changelog, PM_change_log, sites);
    cout<<" complete"<<endl;

    cout<<"Walking through change log ...";
    vector<pair<string, unordered_map<int, vector<BLOCK>>>> PM_schedules = walk_through_changes(PM_schedule, PM_change_log, PM_queue, available_time, num_active_sites);
    cout<<" complete"<<endl;

    ofstream myfilePM;
    myfilePM.open ("FEB15_PM_schedules_LIMITSITES.txt");
 
    for(auto schedule : PM_schedules){
        int filled_hours = 0;
        if(schedule.first != ""){
            myfilePM<<endl;
            myfilePM<<"------------------------------"<<endl;
            myfilePM<< schedule.first<<endl;
            for (auto& [site_id, tests] : schedule.second){
                //myfilePM<<"SITE_"<<site_id<<endl;
                for(auto& test : tests){
                    myfilePM <<test.TR<<", "<<site_id <<", "<< test.start_time<<","<<test.end_time<<endl;
                    if(test.type == Test) filled_hours+=test.duration;
                }
                
            }
            myfilePM <<" hours filled: "<< filled_hours/60<<endl;
        }
    }
    myfilePM.close();


 
 /*
// Initial Schedule Only
    priority_queue<BLOCK, vector<BLOCK>, BLOCK::Comparator> queue; 
    cout<<"Reading initial schedule data ...";
    std::string filename = "/Users/alex/Desktop/Feb15AMList.csv";  // Change this to your CSV file path

    read_schedule_csv(filename, schedule, queue, sites);
    cout<<" complete"<<endl;

    auto results = scheduler(schedule, queue, available_time);
    
    ofstream myfile;
    myfile.open ("AM_GR_15.txt");
 cout<<" complete"<<endl;
        int filled_hours = 0;
        
            myfile<<endl;
            myfile<<"------------------------------"<<endl;
   
            for (auto& [site_id, tests] : results.first){
               // myfile<<"SITE_"<<site_id<<endl;
                for(auto& test : tests){
                    myfile <<test.TR<<", "<<site_id <<", "<< test.start_time<<","<<test.end_time<<endl;
                    if(test.type == Test) filled_hours+=test.duration;
                }
            }
            myfile <<" hours filled: "<< filled_hours/60<<endl;
        
    
    myfile.close();
    */
	return 0;
}
