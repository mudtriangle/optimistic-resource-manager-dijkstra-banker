#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <iterator>
using namespace std;


int num_digs(int num) {
    /*
     * Used for printing. Returns the number of digits
     * of a number (or 1 if it's zero or less).
     */
    if (num <= 0) return 1;
    int num_digs = 0;
    while (num > 0) {
        num = num/10;
        num_digs++;
    }
    return num_digs;
}


vector<string> get_vector(string file_name) {
    /*
     * Used to process input. Returns the the content of a file
     * divided in tokens (elements separated by white space).
     */
    ifstream f(file_name);

    // Return an empty vector if the file has not been found.
    if (!f) {
        cout << "File not found." << endl;
        return {};
    }

    stringstream no_format;
    no_format << f.rdbuf();
    f.close();

    return vector<string>((istream_iterator<string>(no_format)), istream_iterator<string>());

}


class Instruction {
    /*
     * An instruction read by the ResManager, its components are
     * set within the ResManager when the input is processed.
     */
public:
    string type;
    int task_number, delay, resource_type, initial_claim, number_requested, number_released;

    Instruction(string instr, int task, int del, int res, int num_res) {
        // initial_claim, number_requested, and number_released are set to zero.
        type = instr;
        task_number = task;
        delay = del;
        resource_type = res;
        initial_claim = 0;
        number_requested = 0;
        number_released = 0;

        // Depending on the instruction type, set the corresponding value to the last element passed to the constructor.
        if (type == "initiate") {
            initial_claim = num_res;
        } else if (type == "request") {
            number_requested = num_res;
        } else if (type == "release") {
            number_released = num_res;
        }
    }
};


class Task {
    /*
     * A task whose resources are handled by the ResManager, and is
     * created when the ResManager reads the input for the number of
     * tasks in the input.
     */
public:
    int time_taken, waiting_time, curr_instr, curr_wait;
    bool complete, aborted;
    vector<int> resource_types;
    vector<int> initial_claims;
    vector<int> resources_held;

    Task() {
        time_taken = 0;
        waiting_time = 0;
        curr_instr = 0;
        curr_wait = 0;
        complete = false;
        aborted = false;
    }

    void initiate(int res_type, int init_claim) {
        // Code to be executed on an 'initiate' type of instruction.
        resource_types.emplace_back(res_type);
        initial_claims.emplace_back(init_claim);
        resources_held.emplace_back(0);
        curr_instr++;
        time_taken++;
    }

    void granted(int res_type, int num_granted) {
        // Code to be executed when a 'request' type of instruction is approved.
        int ind = 0;
        for (int rt: resource_types) {
            if (rt == res_type) {
                break;
            }
            ind++;
        }

        resources_held[ind] += num_granted;
        curr_instr++;
        time_taken++;
        curr_wait = 0;
    }

    void release(int res_type, int num_to_release) {
        // Code to be executed on a 'release' type of instruction.
        int ind = 0;
        for (int rt: resource_types) {
            if (rt == res_type) {
                break;
            }
            ind++;
        }

        resources_held[ind] -= num_to_release;
        curr_instr++;
        time_taken++;
    }

    void terminate() {
        // Code to be executed on a 'terminate' type of instruction.
        complete = true;
        curr_instr++;
    }

    void wait() {
        // Code to be executed when a 'request' type of instruction is valid but not approved.
        time_taken++;
        waiting_time++;
        curr_wait++;
    }

    void abort() {
        // Code to be executed if the ResManager aborts the task.
        aborted = true;
        time_taken = -1;
        waiting_time = -1;
    }

    void delay() {
        // Code to be executed if a given instruction that references this task is delayed.
        time_taken++;
    }
};


class ResManager {
    /*
     * A resource manager that executes both FIFO and
     * Banker's algorithm.
     */
public:
    vector<string> original_input;
    vector<Task> task_list;
    vector<int> total_res_list;
    vector<int> current_res_list;
    vector<Instruction> instr_list;
    bool deadlock;
    int cycle;

    explicit ResManager(vector<string> input) {
        // Store the original input.
        original_input = input;
        deadlock = false;

        // Create all task instances.
        int num_tasks = stoi(input[0]);
        for (int i = 0; i < num_tasks; i++) {
            task_list.emplace_back(Task());
        }

        // Store resources and resource amounts in a list.
        int res_types = stoi(input[1]);
        for (int i = 0; i < res_types; i++) {
            total_res_list.emplace_back(stoi(input[2 + i]));
        }
        current_res_list = total_res_list;

        // Create all instruction instances.
        for (int i = 2 + res_types; i < input.size(); i = i + 5) {
            instr_list.emplace_back(Instruction(input[i], stoi(input[i + 1]), stoi(input[i + 2]),
                                                stoi(input[i + 3]), stoi(input[i + 4])));
        }

        cycle = 1;
    }

    void execute() {
        // Resolve all tasks using FIFO.
        execute_fifo();
        vector<int> results_fifo = gather_results();

        // Restore all original values as in the input.
        reset();

        // Resolve all tasks using Banker's.
        execute_bankers();
        vector<int> results_bankers = gather_results();

        // Display results on screen.
        print(results_fifo, results_bankers);
    }

private:
    void execute_fifo() {
        // Initiate all the tasks in round zero.
        for (Instruction ins: instr_list) {
            if (ins.type == "initiate") {
                task_list[ins.task_number - 1].initiate(ins.resource_type, ins.initial_claim);
            }
        }

        // Loop through all the instructions until all resources have been terminated or aborted.
        bool finished = false;
        while (!finished) {
            // If not all tasks are waiting, it might not be a deadlock.
            deadlock = true;
            for (Task t: task_list) {
                if (t.complete or t.aborted) {
                    continue;
                } else if (t.curr_wait <= 0) {
                    deadlock = false;
                }
            }

            // Will not do anything unless deadlock is set to true.
            if (deadlock) {
                cout << "Deadlock found during cycles " << cycle - 1 << "-" << cycle << " of FIFO" << endl;
            }
            handle_deadlock();

            int t_num = 1;

            // Lists needed to execute 'release's and 'require's.
            vector<int> released_types;
            vector<int> released_amounts;
            vector<int> reqs;

            // Loops through all the tasks identifying the next instruction to be executed for each task.
            for (Task t: task_list) {
                if (t.aborted or t.complete) {
                    t_num++;
                    continue;
                }

                int i_num = 0;
                int ins_ind = 0;
                for (Instruction ins: instr_list) {
                    if (ins.task_number == t_num) {
                        if (i_num == t.curr_instr) {
                            break;
                        }
                        i_num++;
                    }
                    ins_ind++;
                }

                // Only execute instructions whose delay is equal to zero.
                if (instr_list[ins_ind].delay == 0) {
                    // If the next instruction is a request, add it to the list of requests.
                    if (instr_list[ins_ind].type == "request") {
                        reqs.emplace_back(ins_ind);

                    // If the next instruction is a release, release from the task and add the released
                    // resources to the lists (types and amounts).
                    } else if (instr_list[ins_ind].type == "release") {
                        task_list[t_num - 1].release(instr_list[ins_ind].resource_type,
                                                     instr_list[ins_ind].number_released);
                        released_types.emplace_back(instr_list[ins_ind].resource_type);
                        released_amounts.emplace_back(instr_list[ins_ind].number_released);

                    // If the next instruction is a terminate, terminate the task.
                    } else if (instr_list[ins_ind].type == "terminate") {
                        task_list[t_num - 1].terminate();
                    }

                // If delay is not zero, delay the task.
                } else {
                    instr_list[ins_ind].delay--;
                    task_list[t_num - 1].delay();
                }

                t_num++;
            }

            // Create a list of sorted indices for the 'require' instructions this round.
            vector<int> sorted_reqs;
            vector<int> used_indices;
            for (int i = 0; i < reqs.size(); i++) {
                int max_prio = -1;
                int max_ind = 0;
                for (int j = 0; j < reqs.size(); j++) {
                    Task t = task_list[instr_list[reqs[j]].task_number - 1];
                    if (t.curr_wait > max_prio and
                        find(used_indices.begin(), used_indices.end(), j) == used_indices.end()) {
                        max_prio = t.curr_wait;
                        max_ind = j;
                    }
                }
                sorted_reqs.emplace_back(reqs[max_ind]);
                used_indices.emplace_back(max_ind);
            }

            // Go through the tasks that have requested this cycle and execute them when possible,
            // in order of priority.
            for (int ind: sorted_reqs) {
                bool safe = fifo_check(ind);
                if (safe) {
                    task_list[instr_list[ind].task_number - 1].granted(instr_list[ind].resource_type,
                                                                       instr_list[ind].number_requested);
                    current_res_list[instr_list[ind].resource_type - 1] -= instr_list[ind].number_requested;
                } else {
                    task_list[instr_list[ind].task_number - 1].wait();
                }
            }

            // Add the released resources back to the manager at the end of the cycle.
            for (int i = 0; i < released_types.size(); i++) {
                current_res_list[released_types[i] - 1] += released_amounts[i];
            }

            // If all tasks are finished or aborted, end.
            finished = true;
            for (Task t: task_list) {
                if (t.aborted) {
                    continue;
                }
                if (!t.complete) {
                    finished = false;
                }
            }
            cycle++;
        }
    }

    void handle_deadlock() {
        /*
         * Loops through the tasks when deadlocked and aborts them
         * when necessary, in order of appearance.
         */
        int to_abort = 0;
        while (deadlock) {
            // Verify deadlock.
            int t_num = 1;
            for (Task t: task_list) {
                // Ignore tasks that have already been aborted or completed.
                if (t.aborted or t.complete) {
                    t_num++;
                    continue;
                }

                int i_num = 0;
                int ins_ind = 0;
                for (Instruction ins: instr_list) {
                    if (ins.task_number == t_num) {
                        if (i_num == t.curr_instr) {
                            break;
                        }
                        i_num++;
                    }
                    ins_ind++;
                }

                // Check if the instruction's request can be completed. If so, not a deadlock anymore.
                if (instr_list[ins_ind].number_requested <= current_res_list[instr_list[ins_ind].resource_type - 1]) {
                    deadlock = false;
                    break;
                }

                t_num++;
            }

            // End loop if not a deadlock.
            if (!deadlock) {
                break;
            }

            // Cannot abort tasks that are completed or already aborted.
            if (task_list[to_abort].complete or task_list[to_abort].aborted) {
                to_abort++;
            }

            cout << "    Task " << to_abort + 1 << " aborted" << endl;

            // Abort next task.
            task_list[to_abort].abort();
            for (int i = 0; i < task_list[to_abort].resource_types.size(); i++) {
                current_res_list[task_list[to_abort].resource_types[i] - 1] += task_list[to_abort].resources_held[i];
            }

            to_abort++;
        }
    }

    bool fifo_check(int ind) {
        /*
         * Check if the 'require' instruction can be satisfied or not.
         */
        int task_ind = instr_list[ind].task_number - 1;
        int res_type = instr_list[ind].resource_type;

        if (current_res_list[res_type - 1] < instr_list[ind].number_requested) {
            return false;
        }

        return true;
    }

    void execute_bankers() {
        // Initiate all the tasks in round zero.
        for (Instruction ins: instr_list) {
            if (ins.type == "initiate") {
                // Check whether the claim is valid.
                if (ins.initial_claim <= total_res_list[ins.resource_type - 1]) {
                    task_list[ins.task_number - 1].initiate(ins.resource_type, ins.initial_claim);
                } else {
                    // If not, abort.
                    task_list[ins.task_number - 1].abort();
                    cout << "Banker aborts task " << ins.task_number << " before run begins:" << endl;
                    cout << "    claim for resource " << ins.resource_type << " (" << ins.initial_claim << ") ";
                    cout << "exceeds number of units present (" << total_res_list[ins.resource_type - 1] << ")" << endl;
                }
            }
        }

        // Loop through all the instructions until all resources have been terminated or aborted.
        bool finished = false;
        while (!finished) {
            int t_num = 1;

            // Lists needed to execute 'release's and 'require's.
            vector<int> released_types;
            vector<int> released_amounts;
            vector<int> reqs;

            // Loops through all the tasks identifying the next instruction to be executed for each task.
            for (Task t: task_list) {
                if (t.aborted or t.complete) {
                    t_num++;
                    continue;
                }
                int i_num = 0;
                int ins_ind = 0;
                for (Instruction ins: instr_list) {
                    if (ins.task_number == t_num) {
                        if (i_num == t.curr_instr) {
                            break;
                        }
                        i_num++;
                    }
                    ins_ind++;
                }

                // Only execute instructions whose delay is equal to zero.
                if (instr_list[ins_ind].delay == 0) {
                    // If the next instruction is a request:
                    if (instr_list[ins_ind].type == "request") {
                        int ind = 0;
                        for (int rt: t.resource_types) {
                            if (rt == instr_list[ins_ind].resource_type) {
                                break;
                            }
                            ind++;
                        }
                        int available = t.initial_claims[ind] - t.resources_held[ind];
                        // Check whether it's a valid one.
                        if (instr_list[ins_ind].number_requested <= available) {
                            reqs.emplace_back(ins_ind);

                        // If not, abort.
                        } else {
                            task_list[t_num - 1].abort();
                            for (int i = 0; i < task_list[t_num - 1].resource_types.size(); i++) {
                                released_types.emplace_back(task_list[t_num - 1].resource_types[i]);
                                released_amounts.emplace_back(task_list[t_num - 1].resources_held[i]);
                                cout << "During cycles " << cycle << "-" << cycle + 1 << " of Banker's algorithms";
                                cout << endl << "    Task " << t_num << "'s request exceeds its claim; aborted; ";
                                cout << accumulate(released_amounts.begin(), released_amounts.end(), 0);
                                cout << " units available next cycle" << endl;
                            }
                        }

                    // If it's a release, add to the release lists and release from task.
                    } else if (instr_list[ins_ind].type == "release") {
                        task_list[t_num - 1].release(instr_list[ins_ind].resource_type,
                                                     instr_list[ins_ind].number_released);
                        released_types.emplace_back(instr_list[ins_ind].resource_type);
                        released_amounts.emplace_back(instr_list[ins_ind].number_released);

                    // If next instruction is a terminate, terminate the task.
                    } else if (instr_list[ins_ind].type == "terminate") {
                        task_list[t_num - 1].terminate();
                    }

                // If the delay is not zero, delay the task and decrease the delay in the instruction.
                } else {
                    instr_list[ins_ind].delay--;
                    task_list[t_num - 1].delay();
                }

                t_num++;
            }

            // Create a list of sorted indices for the 'require' instructions this round.
            vector<int> sorted_reqs;
            vector<int> used_indices;
            for (int i = 0; i < reqs.size(); i++) {
                int max_prio = -1;
                int max_ind = 0;
                for (int j = 0; j < reqs.size(); j++) {
                    Task t = task_list[instr_list[reqs[j]].task_number - 1];
                    if (t.curr_wait > max_prio and
                            find(used_indices.begin(), used_indices.end(), j) == used_indices.end()) {
                        max_prio = t.curr_wait;
                        max_ind = j;
                    }
                }
                sorted_reqs.emplace_back(reqs[max_ind]);
                used_indices.emplace_back(max_ind);
            }

            // Go through the tasks that have requested this cycle and execute them when possible,
            // in order of priority.
            for (int ind: sorted_reqs) {

                // Only execute when safe.
                bool safe = bankers_check(ind);
                if (safe) {
                    task_list[instr_list[ind].task_number - 1].granted(instr_list[ind].resource_type,
                                                 instr_list[ind].number_requested);
                    current_res_list[instr_list[ind].resource_type - 1] -= instr_list[ind].number_requested;
                } else {
                    task_list[instr_list[ind].task_number - 1].wait();
                }
            }

            // Add the released resources back to the manager at the end of the cycle.
            for (int i = 0; i < released_types.size(); i++) {
                current_res_list[released_types[i] - 1] += released_amounts[i];
            }

            // If all tasks are completed or aborted, end.
            finished = true;
            for (Task t: task_list) {
                if (t.aborted) {
                    continue;
                }
                if (!t.complete) {
                    finished = false;
                }
            }
            cycle++;
        }
    }

    bool bankers_check(int ind) {
        /*
         * Checks whether the current task requesting resources is allowed to
         * according to Banker's algorithm.
         */
        int task_ind = instr_list[ind].task_number - 1;
        for (int i = 0; i < task_list[task_ind].initial_claims.size(); i++) {
            int res_ind = task_list[task_ind].resource_types[i] - 1;
            if (current_res_list[res_ind] < task_list[task_ind].initial_claims[i]
                                            - task_list[task_ind].resources_held[i]) {
                return false;
            }
        }
        return true;
    }

    vector<int> gather_results() {
        /*
         * Puts the relevant results to be printed in a single one-dimensional vector.
         */
        vector<int> results;
        for (Task t: task_list) {
            results.emplace_back(t.time_taken);
            results.emplace_back(t.waiting_time);
        }
        return results;
    }

    void print(vector<int> fifo, vector<int> bankers) {
        /*
         * Displays the required tables on the screen.
         */

        cout << endl;
        int maxdigs_task = num_digs(task_list.size() - 1);
        int maxdigs_fifo = num_digs(*max_element(fifo.begin(), fifo.end()));
        int maxdigs_bankers = num_digs(*max_element(bankers.begin(), bankers.end()));

        cout << "  ";
        for (int i = 0; i < ((5 + maxdigs_task + 3 * (3 + maxdigs_fifo) + 1) / 2) - 2; i++) {
            cout << " ";
        }
        cout << "FIFO";
        for (int i = 0; i < ((5 + maxdigs_task + 3 * (3 + maxdigs_fifo) + 1) / 2) - 2; i++) {
            cout << " ";
        }
        cout << "  ";
        for (int i = 0; i < ((5 + maxdigs_task + 3 * (3 + maxdigs_bankers) + 1) / 2) - 2; i++) {
            cout << " ";
        }
        cout << "BANKER'S";
        for (int i = 0; i < ((5 + maxdigs_task + 3 * (3 + maxdigs_bankers) + 1) / 2) - 4; i++) {
            cout << " ";
        }
        cout << endl;
        for (int i = 0; i < fifo.size(); i += 2) {
            int digs_task = num_digs((i / 2) + 1);
            int digs_1 = num_digs(fifo[i]);
            int digs_2 = num_digs(fifo[i + 1]);
            int digs_3 = num_digs(bankers[i]);
            int digs_4 = num_digs(bankers[i + 1]);

            cout << "  Task ";
            for (int j = 0; j < maxdigs_task - digs_task; j++) {
                cout << " ";
            }
            cout << (i / 2) + 1;

            if (fifo[i] == -1) {
                for (int j = 0; j < maxdigs_fifo + 2; j++) {
                    cout << " ";
                }
                cout << "aborted";
                for (int j = 0; j < (maxdigs_fifo + 3) * 2 - 5; j++) {
                    cout << " ";
                }
                fifo[i] = 0;
                fifo[i + 1] = 0;
            } else {
                for (int j = 0; j < maxdigs_fifo - digs_1 + 3; j++) {
                    cout << " ";
                }
                cout << fifo[i];
                for (int j = 0; j < maxdigs_fifo - digs_2 + 3; j++) {
                    cout << " ";
                }
                cout << fifo[i + 1];
                for (int j = 0; j < maxdigs_fifo + 3 - num_digs(int(fifo[i + 1] / float(fifo[i]) * 100)); j++) {
                    cout << " ";
                }
                cout << int(fifo[i + 1] / float(fifo[i]) * 100) << "%";
            }

            cout << "  ";

            cout << "  Task ";
            for (int j = 0; j < maxdigs_task - digs_task; j++) {
                cout << " ";
            }
            cout << (i / 2) + 1;

            if (bankers[i] == -1) {
                for (int j = 0; j < maxdigs_bankers + 2; j++) {
                    cout << " ";
                }
                cout << "aborted";
                for (int j = 0; j < (maxdigs_bankers + 3) * 2 - 5; j++) {
                    cout << " ";
                }
                bankers[i] = 0;
                bankers[i + 1] = 0;
            } else {
                for (int j = 0; j < maxdigs_bankers - digs_3 + 3; j++) {
                    cout << " ";
                }
                cout << bankers[i];
                for (int j = 0; j < maxdigs_bankers - digs_4 + 3; j++) {
                    cout << " ";
                }
                cout << bankers[i + 1];
                for (int j = 0;
                     j < maxdigs_bankers + 3 - num_digs(int(bankers[i + 1] / float(bankers[i]) * 100)); j++) {
                    cout << " ";
                }
                cout << int(bankers[i + 1] / float(bankers[i]) * 100) << "%";
            }

            cout << endl;
        }

        int total_1 = 0;
        int total_2 = 0;
        for (int i = 0; i < fifo.size(); i += 2) {
            total_1 += fifo[i];
            total_2 += fifo[i + 1];
        }
        int total_perc_1 = int(total_2 * 100 / float(total_1));

        cout << "  Total";
        for (int j = 0; j < maxdigs_task; j++) {
            cout << " ";
        }
        for (int j = 0; j < maxdigs_fifo - num_digs(total_1) + 3; j++) {
            cout << " ";
        }
        cout << total_1;
        for (int j = 0; j < maxdigs_fifo - num_digs(total_2) + 3; j++) {
            cout << " ";
        }
        cout << total_2;
        for (int j = 0; j < maxdigs_fifo + 3 - num_digs(total_perc_1); j++) {
            cout << " ";
        }
        cout << total_perc_1 << "%";

        cout << "  ";

        int total_3 = 0;
        int total_4 = 0;
        for (int i = 0; i < bankers.size(); i += 2) {
            total_3 += bankers[i];
            total_4 += bankers[i + 1];
        }
        int total_perc_2 = int(total_4 * 100 / float(total_3));

        cout << "  Total";
        for (int j = 0; j < maxdigs_task; j++) {
            cout << " ";
        }
        for (int j = 0; j < maxdigs_bankers - num_digs(total_3) + 3; j++) {
            cout << " ";
        }
        cout << total_3;
        for (int j = 0; j < maxdigs_bankers - num_digs(total_4) + 3; j++) {
            cout << " ";
        }
        cout << total_4;
        for (int j = 0; j < maxdigs_bankers + 3 - num_digs(total_perc_2); j++) {
            cout << " ";
        }
        cout << total_perc_2 << "%";

        cout << endl;
    }

    void reset() {
        /*
         * Sets all the values to their initial positions after an algorithm has
         * already been completed for the next algorithm to do so as well.
         */

        task_list.clear();
        total_res_list.clear();
        current_res_list.clear();
        instr_list.clear();

        int num_tasks = stoi(original_input[0]);
        for (int i = 0; i < num_tasks; i++) {
            task_list.emplace_back(Task());
        }

        int res_types = stoi(original_input[1]);
        for (int i = 0; i < res_types; i++) {
            total_res_list.emplace_back(stoi(original_input[2 + i]));
        }
        current_res_list = total_res_list;

        for (int i = 2 + res_types; i < original_input.size(); i = i + 5) {
            instr_list.emplace_back(Instruction(original_input[i], stoi(original_input[i + 1]),
                                                stoi(original_input[i + 2]), stoi(original_input[i + 3]),
                                                stoi(original_input[i + 4])));
        }

        cycle = 1;
    }
};


int main(int argc, char** argv) {
    string file;
    if (argc == 1) {
        cerr << "No input file given." << endl;
        return 1;
    } else if (argc == 2) {
        file = argv[1];
    }

    vector<string> input_vector = get_vector(file);
    if (input_vector.empty()) {
        cerr << "Invalid input file given." << endl;
        return 2;
    }

    ResManager rm = ResManager(input_vector);
    rm.execute();

    return 0;
}
