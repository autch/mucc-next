#include "codegen.h"
#include <cstdio>
#include <cstring>

namespace {
    void write(FILE* fp, int c)
    {
        fputc(c, fp);
    }

    void write16(FILE* fp, int w)
    {
        fputc(w & 0xFF, fp);
        fputc((w >> 8) & 0xFF, fp);
    }

    void write_n(FILE* fp, void* ptr, size_t size)
    {
        fwrite(ptr, 1, size, fp);
    }
}

uint16_t codegen::write_meta(FILE* fp, const std::string& tag)
{
    uint16_t offset = 0;
    if(auto it = meta.find(tag); it != meta.end()) {
        offset = static_cast<uint16_t>(ftell(fp));
        for(const char* p = it->second.c_str(); *p != '\0'; p++) {
            if(tag == "Title2" && *p == ';') write(fp, '\n');
            else write(fp, *p);
        }
        write(fp, '\0');
    }
    return offset;
}


void codegen::register_meta(const std::string& tag, const std::string& value)
{
    if(auto [iter, success] = meta.insert_or_assign(tag.substr(1), value); !success) {
        printf("Warning: meta tag redefined! [%s]\n", tag.c_str());
    }
}

int codegen::write_pmd(FILE *out_fp)
{
    std::vector<uint16_t> drum_pattern_offsets;
    uint16_t title_offset = 0, title2_offset = 0, part_offsets[MAXPART] = {0}, drum_offset = 0;
    int actual_parts = count_actual_parts();

    memset(part_offsets, 0, sizeof part_offsets);

    // write part definitions
    write(out_fp, 0);
    if (actual_parts < 6)
        write(out_fp, 6);
    else
        write(out_fp, actual_parts);
    // write header
    for(int i = 0; i < actual_parts; i++) {
        write16(out_fp, 0); // placeholder for part offset
    }
    write16(out_fp, 0); // placeholder for drum pattern offset
    write16(out_fp, 0); // placeholder for title offset
    write16(out_fp, 0); // placeholder for title2 offset
    // write title, title2
    title_offset = write_meta(out_fp, "Title");
    title2_offset = write_meta(out_fp, "Title2");
    // write drums
    for(auto& pb: drum_patterns) {
        if(pb.pos > 0) {
            drum_pattern_offsets.push_back(static_cast<uint16_t>(ftell(out_fp)));
            write_n(out_fp, pb.buffer.data(), pb.size);
        } else {
            drum_pattern_offsets.push_back(0);
        }
    }
    // write parts
    for(int part = 0; part < MAXPART; part++) {
        if(part_buffer& pb = parts[part]; pb.pos > 0 && pb.length_written > 0) {
            size_t fp = ftell(out_fp);
            if (fp > 0xffff) {
                printf("Warning: part %c too large to fit in PMD file\n", 'A' + part);
            }
            part_offsets[part] = static_cast<uint16_t>(fp & 0xffff);
            pb.update_part_offset(part_offsets[part]);
            write_n(out_fp, pb.buffer.data(), pb.size);
        } else {
            part_offsets[part] = 0;
        }
    }

    // write drum pattern offsets
    drum_offset = static_cast<uint16_t>(ftell(out_fp));
    for(auto& i: drum_pattern_offsets) {
        write16(out_fp, i);
    }

    // update header
    fseek(out_fp, 2, SEEK_SET);
    for(int i = 0, p = 0; i < MAXPART && p < actual_parts; i++) {
        if(part_offsets[i] > 0) {
            write16(out_fp, part_offsets[i]);
            p++;
        }
    }
    if (actual_parts < 6)
        for (int i = actual_parts; i < 6; i++)
            write16(out_fp, 0);
    write16(out_fp, drum_offset);
    write16(out_fp, title_offset); // title offset
    write16(out_fp, title2_offset); // title2 offset

    return 0;
}

void codegen::report_meta(FILE *fp)
{
    for(auto& [key, value]: meta) {
        fprintf(fp, "META defined: [%s] = [%s]\n", key.c_str(), value.c_str());
    }
}
