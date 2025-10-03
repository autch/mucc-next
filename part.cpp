#include <cctype>
#include <cstdio>
#include <cstring>
#include <cmath>

#include "part.h"

char mml_ctx::getchar()
{
    while(true) {
        if(char c = *p++; c != '\0')
            return c;
        if(macro_sp == 0)
            return 0;
        pop_macro();
    }
}

int mml_ctx::read_number(int* out, int* is_relative)
{
    int sign = 1;
    *is_relative = 0;
    if(peek() == '+' || peek() == '-') {
        if(peek() == '-')
            sign = -1;
        *is_relative = 1;
        getchar();
    }
    if(!isdigit(peek())) {
        return 0; // no number found
    }
    int value = 0;
    while(isdigit(peek())) {
        value = value * 10 + (getchar() - '0');
    }
    *out = value * sign;
    while(isspace(peek()))
        getchar(); // skip whitespace
    return 1;
}

int mml_ctx::read_numbers(int* out, int count)
{
    int rel = 0, nums_read = 0;
    for(int i = 0; i < count; i++) {
        if(!read_number(out + i, &rel)) {
            return nums_read; // failed to read number
        }
        nums_read++;
        if(peek() == ',' && i < count - 1)
            getchar(); // skip comma
        while(isspace(peek()))
            getchar(); // skip whitespace
    }
    return nums_read;
}

int convert_length_to_clock(int len)
{
    // assuming 4/4 time signature and 24 clocks per quarter note
    if(len <= 0) 
    {
        printf("warning: length too short (%d), setting to 1\n", len);
        return 1;
    }
    if(len > TIMEBASE) {
        printf("warning: length too long (%d), limiting to 96\n", len);
        len = TIMEBASE;
    }
    return TIMEBASE / len; // 96 = 24 * 4
}

int mml_ctx::read_length(int* out, int* is_relative)
{
    if(peek() == '%') {
        // direct clock number
        getchar(); // consume '%'
        return read_number(out, is_relative);
    } else {
        if(!isdigit(peek())) {
            return 0; // no number found
        }
        int total = 0;
        while(true) {
            int value = 0;
            while(isdigit(peek())) {
                value = value * 10 + (getchar() - '0');
            }
            value = convert_length_to_clock(value);
            if(peek() == '.') {
                // dot length
                int x = value;
                do {
                    value += (x >>= 1);
                    getchar();
                }while(peek() == '.');
            }
            total += value;
            if(peek() == '^') {
                getchar();
                continue;
            }
            break;
        }
        *out = total;
        *is_relative = 0;
        while(isspace(peek()))
            getchar(); // skip whitespace
        return 1;
    }
}

void mml_ctx::gen_note(mml_part &mp, int code, part_buffer &pb)
{
    int len, rel;
    if(int ret = read_length(&len, &rel); ret == 0)
        len = mp.len + mp.len1;

    if(peek() == '&') {
        getchar();
        if(mp.xxleg < 2) mp.xxleg = 2; // tie
    }

    if(mp.porsw == 2) {
		int x = static_cast<int>(log(fabs(mp.pornote2 - mp.pornote1) / len) / log(2.0));
		x <<= 4;
		x += 128;
        
        pb.write(CCD_PPA);
        pb.write(mp.pornote2 - mp.pornote1);
        pb.write(x);

        mp.porsw = 0;
        
        pb.write(CCD_PON);
    }

    if(mp.xxleg) mp.xxleg--;
    if(mp.xxleg) {
        if(!(mp.leg & 1)) {
            pb.write(CCD_LEG);
            mp.leg |= 1;
        }
    } else {
        if(mp.leg & 1) {
            pb.write(CCD_LOF);
            mp.leg &= ~1;
        }
    }

    if ( !mp.porsw ) {
		if ( mp.xlen != len ) {
			mp.xlen = len;
            pb.write_length(len);
		}
        mp.current_tick() += len;

        pb.write((code >= 0 && code < 96) ? 128 + code : CCD_RRR);
	} else {
		mp.pornote2 = mp.pornote1;
		mp.pornote1 = code;
	}
}

int mml_ctx::call_macro(mml_part &mp, part_buffer &pb)
{
    char c;
    char macro_name[MAXMACRONAME];
    int i = 0;
    while((c = peek()) != '\0' && isalpha(c)) {
        macro_name[i++] = c;
        getchar();
    }
    macro_name[i] = '\0';
    if(auto it = macro.find(std::string(macro_name)); it != macro.end()) {
        if(macro_sp >= MACRONEST) {
            printf("Macro stack overflow.\n");
            return -1;
        }
        push_macro();
        set_p(const_cast<char *>(it->second.c_str()));
    } else if(mp.tr_attr & 1) {
        if(auto dit = drummacro.find(std::string(macro_name)); dit != drummacro.end()) {
            // write(dit->second.first - 0x80) // drum index
            gen_note(mp, dit->second.index - 0x80, pb);
            mp.xnote = dit->second.index - 0x80;
        } else {
            printf("Drum macro not found: [%s]\n", macro_name);
        }
    } else {
        printf("Macro isn't found: [%s], note: cannot call drum macro in tracks other than =1'ed\n", macro_name);
    }
    return 0;
}

void mml_ctx::register_macro(int lineno, const std::string& name, const std::string& definition)
{
    if(name[1] == '!') {
        // drum macro
        if(drummacro_count >= MAXDRUMMACRO) {
            printf("Too many drum macros (max %d).\n", MAXDRUMMACRO);
            return;
        }
        if(auto [iter, success] = drummacro.insert_or_assign(
            name.substr(2), drummacro_item{drummacro_count + 0x80, lineno, definition});
            !success) {
            printf("Warning: drum macro redefined! [%s]\n", name.c_str());
        }
        drummacro_count++;
    } else {
        // normal macro
        if(auto [iter, success] = macro.insert_or_assign(name.substr(1), definition);
            !success) {
            printf("Warning: macro redefined! [%s]\n", name.c_str());
        }
    }
}

int mml_ctx::parse_partline(char* part_token, int lineno, char* line, codegen& cg)
{
    char part_name[2];

    for(char* pt = part_token; *pt != '\0'; pt++) {
        if(strchr(part_token, *pt) != pt) {
            printf("duplicate part definition: [%c][%s]\n", *pt, part_token);
            return -1;
        }
        char part = *pt;
        part_name[0] = part;
        part_name[1] = '\0';
        // printf("\tParsing for part [%c]\n", part);

        // set part context to p
        mml_part* mp = parts + (part - 'A');
        part_buffer* pb = &cg.get_part(part - 'A');
        set_p(line);
        
        parse_partdef(part_name, lineno, *mp, *pb);
    }
    return 0;
}

int mml_ctx::parse_drumline(codegen& cg)
{
    char drum_token[MAXMACRONAME];

    for(auto &[name, item] : drummacro) {
        const std::string& definition = item.definition;
        mml_part dummy_part; // dummy part context for drum pattern
        part_buffer pb;

        set_p(const_cast<char *>(definition.c_str()));

        snprintf(drum_token, sizeof drum_token, "!!%s", name.c_str());
        parse_partdef(drum_token, item.lineno, dummy_part, pb);

        pb.write(CCD_END);
        pb.size = pb.pos;

        cg.add_drum_buffer(pb);
    }
    return 0;
}

int mml_ctx::end_mml(codegen& cg)
{
    for(int part = 0; part < MAXPART; part++) {
        if(part_buffer& pb = cg.get_part(part); pb.pos > 0) {
            if(pb.loop_pos > 0) {
                pb.write(CCD_JMP);
                pb.loop_addr_pos = pb.addr();           // needs relocation
                pb.write16(static_cast<uint16_t>(pb.loop_pos));      // also needs relocation
            } else {
                pb.write(CCD_END);
            }
            pb.size = pb.pos;
        }
    }
    parse_drumline(cg);
    return 0;
}

void mml_ctx::report_macro(FILE *fp)
{
    for(auto& [key, value]: macro) {
        fprintf(fp, "MACRO defined: [%s] = [%s]\n", key.c_str(), value.c_str());
    }
}

void mml_ctx::report_drummacro(FILE *fp)
{
    for(auto& [key, value]: drummacro) {
        fprintf(fp, "DRUMMACRO defined: [%s] = (%2d)[%s]\n", key.c_str(), value.index, value.definition.c_str());
    }
}
