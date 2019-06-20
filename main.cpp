#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <queue>

using namespace std;

vector<string> split(const string& line, char ch) {
    vector<string> res;
    string s = line;
    while (s.length() > 0) {
        auto t = s.find(ch);
        if (t == string::npos) break;
        res.push_back(s.substr(0, t));
        s = s.substr(t + 1);
    }
    res.push_back(s);
    return res;
}


class Instruction {
public:

    class Source {
    public:
        enum SourceType {
            Integer,
            Regester,
        };

        SourceType type;
        long val;

        Source() : type(Integer), val(0) {}

        void debug() {
            if (type == Integer) {
                cout << "Integer(" << val << ")";
            }
            else {
                cout << "Regester[" << val << "]";
            }
        }

        static Source get_source(string s) {
            Source res;
            if (s[0] == '0' && s[1] == 'x') {
                res.type = Integer;
                char* pEnd;
                res.val = strtol(s.c_str(), &pEnd, 0);
                if (res.val > 1ll << 31) {
                    res.val -= 1ll << 32;
                }
            }
            else if (s[0] == 'F') {
                res.type = Regester;
                char* pEnd;
                res.val = strtol(s.substr(1).c_str(), &pEnd, 10);
            }
            return res;
        }
    };

    string initial_line;
    string opt;
    Source x1, x2, x3;
    int cycles;
    int issue_cycle, gowait_cycle, exec_cycle, wb_cycle;

    Instruction() {
        issue_cycle = -1;
        gowait_cycle = -1;
        exec_cycle = -1;
        wb_cycle = -1;
    }

    void debug() {
        cout << "Instruction:  opt = " + opt + "  x1 = ";
        x1.debug();
        cout << "  x2 = ";
        x2.debug();
        cout << "  x3 = ";
        x3.debug();
        cout << "  cycles = " << cycles << endl;
    }

    static int get_cycles(string opt) {
        if (opt == "LD") return 3;
        if (opt == "JUMP") return 1;
        if (opt == "ADD" || opt == "SUB") return 3;
        if (opt == "MUL") return 4;
        if (opt == "DIV") return 4;
        exit(1);
    }

    static Instruction parse(const string& line) {
        Instruction inst;
        inst.initial_line = line;
        vector<string> a = split(line, ',');
        inst.opt = a[0];
        inst.x1 = Source::get_source(a[1]);
        inst.x2 = Source::get_source(a[2]);
        if (a.size() > 3) {
            inst.x3 = Source::get_source(a[3]);
        }
        inst.cycles = get_cycles(inst.opt);
        return inst;
    }
};


class TempRegister {
public:
    int id;
    long val;
    bool ok;

    TempRegister() {
        id = get_id();
        val = 0;
        ok = false;
    }
    TempRegister(long x) {
        id = get_id();
        val = x;
        ok = true;
    }

    void put(long x) {
        val = x;
        ok = true;
    }

    void debug() {
        cout << "(id=" << id << ", val=" << val << ", ok=" << ok << ")";
    }

    static int get_id() {
        static int cnt = 0;
        return cnt++;
    }
};


class ReservationStation {
public:
    string opt;
    bool busy;
    TempRegister* src1;
    TempRegister* src2;
    TempRegister* dst;
    enum RS_state {
        IDLE,
        ISSUE,
        WAIT,
        EXEC,
        WB,
    } state;
    int cycle_left;
    Instruction* inst;
    
    ReservationStation() {
        opt = "";
        busy = false;
        src1 = NULL;
        src2 = NULL;
        dst = NULL;
        state = IDLE;
        cycle_left = 0;
        inst = NULL;
    }

    bool ready() {
        if (src1 != NULL && !src1->ok) return false;
        if (src2 != NULL && !src2->ok) return false;
        return true;
    }

    void clear() {
        opt = "";
        busy = false;
        src1 = NULL;
        src2 = NULL;
        dst = NULL;
        state = IDLE;
        cycle_left = 0;
        inst = NULL;
    }

    void get_result() {
        if (opt == "LD") {
            dst->put(src1->val);
        }
        else if (opt == "ADD") {
            dst->put(src1->val + src2->val);
        }
        else if (opt == "SUB") {
            dst->put(src1->val - src2->val);
        }
        else if (opt == "MUL") {
            dst->put(src1->val * src2->val);
        }
        else if (opt == "DIV") {
            if (src2->val == 0) {
                dst->put(src1->val);
            }
            else {
                dst->put(src1->val / src2->val);
            }
        }
        else if (opt == "JUMP") {
            if (src1->val != src2->val) {
                dst->put(1);
            }
        }
        else {
            exit(1);
        }
    }

    void debug() {
        string go[] = {"IDLE", "ISSUE", "WAIT", "EXEC", "WB"};
        cout << "  busy: " << busy << "  opt: " << opt << "  state: " << go[state];
        cout << "  cycle_left: " << cycle_left;
        if (src1 != NULL) {
            cout << "  src1: ";
            src1->debug();
        }
        if (src2 != NULL) {
            cout << "  src2: ";
            src2->debug();
        }
        if (dst != NULL) {
            cout << "  dst: ";
            dst->debug();
        }
        cout << endl;
    }
};


class TomasuloSimulator {
public:
    vector<Instruction> instructions;
    int PC;
    int cycle;
    int registers[32];
    TempRegister* register_result[32];
    vector<ReservationStation> Ars;
    vector<ReservationStation> Mrs;
    vector<ReservationStation> LB;
    vector<ReservationStation*> all_RS;
    int add_left;
    int mult_left;
    int load_left;
    queue<ReservationStation*> add_queue;
    queue<ReservationStation*> mult_queue;
    queue<ReservationStation*> load_queue;
    bool jump;

    TomasuloSimulator() {
        add_left = 3;
        mult_left = 2;
        load_left = 2;
        Ars = vector<ReservationStation>(6);
        Mrs = vector<ReservationStation>(3);
        LB = vector<ReservationStation>(3);
        for (ReservationStation& RS : Ars) {
            all_RS.push_back(&RS);
        }
        for (ReservationStation& RS : Mrs) {
            all_RS.push_back(&RS);
        }
        for (ReservationStation& RS : LB) {
            all_RS.push_back(&RS);
        }
        for (int i = 0; i < 32; i++) {
            register_result[i] = new TempRegister(0);
        }
    };

    void read_instructions(string path) {
        ifstream fin(path);
        string line;
        while (fin >> line) {
            Instruction inst = Instruction::parse(line);
            // inst.debug();
            instructions.push_back(inst);
        }
    }

    bool check_stop() {
        for (ReservationStation* RS : all_RS) {
            if (RS->busy) return false;
        }
        return PC >= instructions.size();
    }

    void issue() {
        if (PC < instructions.size() && !jump) {
            Instruction& inst = instructions[PC];
            string opt = inst.opt;
            if (opt == "LD") {
                for (ReservationStation& RS : LB) {
                    if (!RS.busy) {
                        RS.busy = true;
                        RS.opt = opt;
                        RS.src1 = new TempRegister(inst.x2.val);
                        RS.dst = new TempRegister();
                        RS.state = ReservationStation::ISSUE;
                        RS.cycle_left = inst.cycles;
                        RS.inst = &inst;
                        register_result[inst.x1.val] = RS.dst;
                        if (inst.issue_cycle == -1) {
                            inst.issue_cycle = cycle;
                        }
                        PC += 1;
                        break;
                    }
                }
            }
            else if (opt == "JUMP") {
                for (ReservationStation& RS : Ars) {
                    if (!RS.busy) {
                        RS.busy = true;
                        RS.opt = opt;
                        RS.src1 = new TempRegister(inst.x1.val);
                        RS.src2 = register_result[inst.x2.val];
                        RS.dst = new TempRegister(inst.x3.val);
                        RS.state = ReservationStation::ISSUE;
                        RS.cycle_left = inst.cycles;
                        RS.inst = &inst;
                        if (inst.issue_cycle == -1) {
                            inst.issue_cycle = cycle;
                        }
                        jump = true;
                        break;
                    }
                }
            }
            else if (opt == "ADD" || opt == "SUB") {
                for (ReservationStation& RS : Ars) {
                    if (!RS.busy) {
                        RS.busy = true;
                        RS.opt = opt;
                        RS.src1 = register_result[inst.x2.val];
                        RS.src2 = register_result[inst.x3.val];
                        RS.dst = new TempRegister();
                        RS.state = ReservationStation::ISSUE;
                        RS.cycle_left = inst.cycles;
                        RS.inst = &inst;
                        register_result[inst.x1.val] = RS.dst;
                        if (inst.issue_cycle == -1) {
                            inst.issue_cycle = cycle;
                        }
                        PC += 1;
                        break;
                    }
                }
            }
            else {
                for (ReservationStation& RS : Mrs) {
                    if (!RS.busy) {
                        RS.busy = true;
                        RS.opt = opt;
                        RS.src1 = register_result[inst.x2.val];
                        RS.src2 = register_result[inst.x3.val];
                        RS.dst = new TempRegister();
                        RS.state = ReservationStation::ISSUE;
                        RS.cycle_left = inst.cycles;
                        RS.inst = &inst;
                        register_result[inst.x1.val] = RS.dst;
                        if (inst.issue_cycle == -1) {
                            inst.issue_cycle = cycle;
                        }
                        PC += 1;
                        break;
                    }
                }
            }
        }
    }

    void work0(ReservationStation& RS) {
        if (RS.state == ReservationStation::EXEC) {
            if (RS.cycle_left == 0) {
                RS.get_result();
                RS.state = ReservationStation::WB;
                if (RS.inst->wb_cycle == -1) {
                    RS.inst->wb_cycle = cycle;
                }
                if (RS.opt == "JUMP") {
                    PC = PC + RS.dst->val;
                    jump = false;
                }
                if (RS.opt == "LD") {
                    load_left++;
                }
                else if (RS.opt == "MUL" || RS.opt == "DIV") {
                    mult_left++;
                }
                else {
                    add_left++;
                }
            }
        }
    }

    void work1(ReservationStation& RS) {
        if (!RS.busy) return;
        if (RS.state == ReservationStation::ISSUE) {
            if (RS.ready()) {
                RS.state = ReservationStation::WAIT;
                if (RS.opt == "LD") {
                    load_queue.push(&RS);
                } else if (RS.opt == "JUMP" || RS.opt == "ADD" || RS.opt == "SUB") {
                    add_queue.push(&RS);
                } else {
                    mult_queue.push(&RS);
                }
                RS.inst->gowait_cycle = cycle;
            }
        }
    }

    void work2(ReservationStation& RS) {
        if (RS.state == ReservationStation::EXEC) {
            if (RS.cycle_left > 0) {
                RS.cycle_left--;
                if (RS.cycle_left == 0 && RS.inst->exec_cycle == -1) {
                    RS.inst->exec_cycle = cycle;
                }
            }
        }
    }

    void goto_exec(int& hardware, queue<ReservationStation*>& Q) {
        while (hardware > 0 && !Q.empty()) {
            hardware--;
            ReservationStation* RS = Q.front();
            Q.pop();
            RS->state = ReservationStation::EXEC;
            if (RS->opt == "DIV" && RS->src2->val == 0) {
                RS->cycle_left = 1;
            }
            if (RS->inst->gowait_cycle < cycle) {
                RS->cycle_left--;
            }
        }
    }

    void work_queue() {
        goto_exec(add_left, add_queue);
        goto_exec(mult_left, mult_queue);
        goto_exec(load_left, load_queue);
    }

    void clear() {
        for (ReservationStation* RS : all_RS) {
            if (RS->state == ReservationStation::WB) {
                RS->clear();
            }
        }
    }

    void debug() {
        string names[] = {"Ars1", "Ars2", "Ars3", "Ars4", "Ars5", "Ars6", "Mrs1", "Mrs2", "Mrs3",
                          "LB1", "LB2", "LB3"};
        cout << "Reservation Stations:" << endl;
        for (int i = 0; i < 12; i++) {
            auto RS = all_RS[i];
            if (!RS->busy) continue;
            cout << names[i] << "  ";
            RS->debug();
        }
    }

    void show_result() {
        cout << endl << endl << "Final result:" << endl;
        for (auto inst : instructions) {
            cout << left << setw(35) << inst.initial_line;
            cout << inst.issue_cycle << "  " << inst.exec_cycle;
            cout << "  " << inst.wb_cycle << endl;
        }
        cout << endl << "Registers:" << endl;
        for (int i = 0; i < 32; i++) {
            if (registers[i] == 0) continue;
            cout << "Reg[" << i << "] = " << registers[i] << endl;
        }
    }

    void run() {
        PC = 0;
        cycle = 0;
        for (;;) {
            if (check_stop()) break;
            cycle += 1;
            printf("\n------------------------------------ cycle = %d ------------------------------------\n", cycle);
            for (ReservationStation* RS : all_RS) {
                work0(*RS);
            }
            issue();
            for (ReservationStation* RS : all_RS) {
                work2(*RS);
            }
            for (ReservationStation* RS : all_RS) {
                work1(*RS);
            }
            work_queue();
            clear();
            debug();
        }
        for (int i = 0; i < 32; i++) {
            registers[i] = register_result[i]->val;
        }
        show_result();
    }
};


int main(int argc, char** argv) {
    string path = "../../test0.nel";
    TomasuloSimulator simulator;
    simulator.read_instructions(path);
    simulator.run();
    return 0;
}