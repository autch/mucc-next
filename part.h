#if !defined(PART_H)
#define PART_H

#include <cstdint>

#include <string>
#include <map>
#include <array>
#include <vector>

#include "mucc_def.h"
#include "part_buffer.h"

class codegen; // forward declare to avoid including codegen.h in this header

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

    std::array<int, MAXNEST> nestdata{};
    int nestlevel{0};

    auto& current_tick() { return tickstack.at(ticklevel); }
    auto& ticks_till_break() { return tickstack.at(ticklevel - 1); }

    // Safe operations for tick stack to avoid direct index arithmetic in callers.
    void push_tick_zero()
    {
        ++ticklevel;
        if (ticklevel >= static_cast<int>(tickstack.size())) {
            tickstack.resize(static_cast<size_t>(ticklevel) + 1, 0u);
        }
        tickstack.at(ticklevel) = 0u;
    }

    unsigned pop_tick_get()
    {
        unsigned v = tickstack.at(ticklevel);
        --ticklevel;
        return v;
    }

    void save_tick_at_loop()
    {
        tick_at_loop = current_tick();
    }

    [[nodiscard]]
    unsigned get_tick_at_loop() const
    {
        return tick_at_loop;
    }
private:
    // Use a vector for tickstack to allow safe resizing and clearer semantics.
    // Initialize with an explicit constructor so the vector has (MAXNEST*2) elements initialized to 0.
    std::vector<unsigned> tickstack = std::vector<unsigned>(MAXNEST * 2, 0u);
    int ticklevel{1};
    unsigned tick_at_loop{0};
};

class mml_ctx
{
    mml_part parts[MAXPART]{}; // A-Z plus some extra

    int tempo1 = 0, tempo{120}; // tempo: absolute, tempo1: relative

    struct drummacro_item {
        int index;
        int lineno;
        std::string definition;
    };

    std::map<std::string, drummacro_item> drummacro; // key -> (index, definition)
    int drummacro_count = 0;
    std::map<std::string, std::string> macro;   // key -> definition

    char* macro_stack[MACRONEST]{nullptr};
    int macro_sp{0};

    char* p{nullptr}; // current position in buffer
    uint32_t partflags = 0;

    void push_macro()
    {
        if(macro_sp >= MACRONEST) {
            printf("Macro stack overflow.\n");
            return;
        }
        macro_stack[macro_sp++] = p;
    }
    void pop_macro()
    {
        if(macro_sp <= 0) {
            printf("Macro stack underflow.\n");
            return;
        }
        p = macro_stack[--macro_sp];
    }

    void set_p(char* new_p) { p = new_p; }
    char getchar();
    [[nodiscard]] char peek() const { return *p; }
    int read_number(int* out, int* is_relative);
    int read_numbers(int* out, int count);
    int read_length(int* out, int* is_relative);

    int call_macro(mml_part &mp, part_buffer &pb);
    void gen_note(mml_part &mp, int code, part_buffer &pb);

    int parse_drumline(codegen& cg);
    int parse_partdef(char* part_name, int lineno, mml_part &mp, part_buffer &pb);
public:
    void register_macro(int lineno, const std::string& name, const std::string& definition);

    int parse_partline(char* part_token, int lineno, char* line, codegen& cg);
    int parse_wildcardline(int lineno, char* line, codegen& cg);
    int end_mml(codegen& cg);

    auto& get_part(int part) { return parts[part]; }

    void report_macro(FILE* fp);
    void report_drummacro(FILE* fp);
};

#endif // !defined(PART_H)
