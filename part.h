#if !defined(PART_H)
#define PART_H

#include <cstdint>

#include <string>
#include <map>
#include <array>
#include <deque>
#include <stack>

#include "mucc_def.h"
#include "part_buffer.h"

class codegen;

struct mml_part
{
    int xlen{0};
    uint8_t tr_attr{0};
    uint8_t xnote{0};

    uint8_t oct{0};
    uint8_t len{0};
    uint8_t vol{0};
    int8_t det{0};
    uint8_t leg{0};
    uint8_t xxleg{0};
    uint8_t porsw{0};
    uint8_t pornote1{0};
    uint8_t pornote2{0};
    int8_t oct1{0};
    int8_t len1{0};
    int8_t vol1{0};
    int8_t det1{0};
    int8_t trs{0};
    uint8_t def_exp{1};  // default step of relative expression command `(/)`
    int exp_mul{1};      // rel. expressions with number, i.e. `(n` or `)n`, are multiplied by this value

    void push_loop_addr(uint16_t addr) { nestdata.push(addr); }
    uint16_t pop_loop_addr()
    {
        uint16_t addr = nestdata.top();
        nestdata.pop();
        return addr;
    }
    size_t loop_nest_level() const { return nestdata.size(); }

    using tickitem = std::pair<unsigned, unsigned>;
    auto& current_tick() { return tickstack.top().first; }
    auto& ticks_till_break() { return tickstack.top().second; }

    void push_tick() { tickstack.emplace(0u, 0u); }
    tickitem pop_tick()
    {
        tickitem v = tickstack.top();
        tickstack.pop();
        return v;
    }

    void save_tick_at_loop() { tick_at_loop = current_tick(); }
    [[nodiscard]]
    unsigned get_tick_at_loop() const { return tick_at_loop; }

    mml_part() {
        push_tick();
    }
private:
    std::stack<int> nestdata; // stack of loop start addresses
    std::stack<tickitem> tickstack;
    unsigned tick_at_loop{0};
};

class mml_ctx
{
    std::array<mml_part, MAXPART> parts{}; // A-Z plus some extra

    int tempo1 = 0, tempo{120}; // tempo: absolute, tempo1: relative

    struct drummacro_item {
        int index;
        int lineno;
        std::string definition;
    };

    std::map<std::string, drummacro_item> drummacro; // key -> (index, definition)
    int drummacro_count = 0;

    struct macro_item {
        int lineno;
        std::string definition;
    };
    std::map<std::string, macro_item> macro;   // key -> (lineno, definition)

    struct macro_stack_item {
        int lineno;
        std::string_view line;
        int pos;
    };
    std::stack<macro_stack_item> macro_stack;

    int lineno = 0;
    std::string_view line;
    int pos{0};
    uint32_t partflags = 0;

    void push_macro()
    {
        if(macro_stack.size() >= MACRONEST) {
            printf("Macro stack overflow.\n");
            return;
        }
        macro_stack.push({lineno, line, pos});
    }
    void pop_macro()
    {
        if(macro_stack.empty()) {
            printf("Macro stack underflow.\n");
            return;
        }
        auto [lineno, line, pos] = macro_stack.top();
        this->lineno = lineno;
        this->line = line;
        this->pos = pos;
        macro_stack.pop();
    }

    void set_p(std::string_view new_line, int lineno)
    {
        line = new_line;
        pos = 0;
        this->lineno = lineno;
    }
    char getchar();
    [[nodiscard]] char peek() const
    {
        if(pos >= line.size()) return '\0';
        return line[pos];
    }
    int read_number(int& out, int& is_relative);
    int read_numbers(int* out, int count);
    int read_length(int& out, int& is_relative);

    int call_macro(mml_part &mp, part_buffer &pb);
    void gen_note(mml_part &mp, int code, part_buffer &pb);

    int parse_drumline(codegen& cg);
    int parse_partdef(std::string_view part_name, mml_part &mp, part_buffer &pb);
public:
    void register_macro(int lineno, const std::string& name, const std::string& definition);
    void register_drummacro(int lineno, const std::string& name, const std::string& definition);

    int parse_partline(std::string_view part_token, int lineno, std::string_view line, codegen& cg);
    int parse_wildcardline(int lineno, std::string_view line, codegen& cg);
    int end_mml(codegen& cg);

    auto& get_part(int part) { return parts[part]; }

    void report_macro(FILE* fp);
    void report_drummacro(FILE* fp);
};

#endif // !defined(PART_H)
