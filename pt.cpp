#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include "mucc_def.h"
#include "file.h"
#include "part.h"
#include "codegen.h"

int parse_meta(file &f, codegen& cg)
{
    if(f.read_token(is_meta_token) == 1) {
        // printf("\tMETA: [%s][%s]\n", f->last_token, f->p);
        cg.register_meta(std::string(f.last_token), std::string(f.p));
        return 1;
    } else {
        printf("No meta token found.\n");
    }
    return 0;
}

int parse_macro(file &f, mml_ctx& mml)
{
    if(f.read_token(is_macro_name) == 1) {
        // printf("\tMACDEF: [%s][%s]\n", f->last_token, f->p);
        mml.register_macro(f.line_number, std::string(f.last_token), std::string(f.p));
        return 1;
    } else {
        printf("No macro token found.\n");
    }
    return 0;
}

int is_part_line(char* p)
{
    char* pp = p;
    char* part_end = nullptr;

    while(*pp && isupper(*pp))
        pp++;
    
    if(pp == p)
        return 0; // no uppercase letters at start

    part_end = pp;

    while(*pp && isspace(*pp))
        pp++;

    if(pp == part_end && *pp != '\0')
        return 0; // no space after part token, but not end of line

    return part_end - p; // return length of part-line token
}

int parse_line(file &f, mml_ctx& mml, codegen& cg)
{
    switch(*f.p) {
        case '\0':
            break;
        case '#':
            parse_meta(f, cg);
            break;
        case '!':
            parse_macro(f, mml);
            break;
        default:
        {
            int len;
            if((len = is_part_line(f.p)) > 0) {
                char part_token[MAXPARTNAME];
                snprintf(part_token, sizeof part_token, "%.*s", (int)len, f.p);
                f.p += len;
                while(*f.p && isspace(*f.p))
                    f.p++;
                // printf("\tPART: [%s][%s]\n", part_token, f->p);
                mml.parse_partline(part_token, f.line_number, f.p, cg);

                if(f.p > f.buffer + sizeof(f.buffer) || f.p < f.buffer) {
                    // f->p may point to somewhere in macro definition
                    printf("Error: pointer out of buffer range.\n");
                    return -1;
                }
            } else {
                printf("Unknown line format: [%s]\n", f.p);
            }
        }
    }
    return 0;
}


int main(int ac, char** av)
{
    file f;
    codegen cg;
    mml_ctx mml;
    char* out_filename = nullptr;
    
    if (ac < 2) {
        fprintf(stderr, "Usage: %s <filename> [out_filename]\n", av[0]);
        return EXIT_FAILURE;
    }
    if (f.open(av[1]) != 0) {
        perror("file_open");
        return EXIT_FAILURE;
    }
    if(ac >= 3) {
        out_filename = av[2];
    }

    while (f.readline() != nullptr) {
        if(*f.p == '\0')
            continue; // skip empty lines
        // printf("READ[%3d]: [%s]\n", f.line_number, f.p);
        parse_line(f, mml, cg);
    }
    f.close();

    mml.end_mml(cg);

    {
        FILE* out_fp;
        if(out_filename) {
            out_fp = fopen(out_filename, "wb");
            if(!out_fp) {
                perror("fopen for output");
                return EXIT_FAILURE;
            }
        } else {
            out_fp = stdout;
        }

        cg.write_pmd(out_fp);

        if(out_fp != stdout) {
            fclose(out_fp);
            out_fp = nullptr;
        }
    }

    cg.report_meta(stdout);
    mml.report_macro(stdout);
    mml.report_drummacro(stdout);

    for(int part = 0; part < MAXPART; part++) {
        mml_part& mp = mml.get_part(part);
        part_buffer& pb = cg.get_part(part);
        unsigned ticks = mp.current_tick();
        if(ticks > 0) {
            printf("PART %c: total %4u, at loop %4u, %5llu bytes\n",
                'A' + part, ticks, mp.tick_at_loop, pb.size);
        }
    }

    return EXIT_SUCCESS;
}
