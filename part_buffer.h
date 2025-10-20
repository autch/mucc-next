// filepath: c:\Users\nishimura\src\mucc-next\part_buffer.h
#ifndef PART_BUFFER_H
#define PART_BUFFER_H

#include <cstdint>
#include <vector>
#include <cstdio>

#include "mucc_def.h"
#include "reserved.h" // control codes (CCD_*)

// contains generated sequence data for one part
struct part_buffer
{
    std::vector<uint8_t> buffer; // sequence data buffer, dynamically extended
    size_t size{0};    // actual size of data in buffer
    size_t pos{0};      // writing position, offset from buffer

    size_t loop_pos{0}; // position of loop point `L`, offset from buffer. 0 means no loop point
    size_t loop_addr_pos{0}; // position of jump instruction to loop_pos, usually at end of part, offset from buffer. 0 means no loop point

    unsigned length_written{0};

    part_buffer() = default;

    void write(uint8_t byte)
    {
        if(pos >= buffer.size()) {
            // extend buffer
            buffer.resize(buffer.size() + 128);
        }
        buffer[pos++] = byte;
        if(pos > size) size = pos;
    }
    void write16(uint16_t word)
    {
        write(word & 0xFF);
        write((word >> 8) & 0xFF);
    }
    void write_at(uint16_t at, uint8_t byte)
    {
        if(at < buffer.size()) {
            buffer[at] = byte;
            if(at >= size) size = at + 1;
        } else {
            buffer.resize(at + 128);
            buffer[at] = byte;
            size = at + 1;
        }
    }
    void write_length(int length)
    {
        if(length < 127) write(length);
        else if (length < 256) { write(CCD_LEX); write(length); }
        else if (length < 127 + 256) { write(CCD_LEX); write(length - 256); }
        else { printf("<length too large>"); }
    }
    void write2(uint8_t cmd, int param)
    {
        write(cmd);
        write(param);
    }
    void write_n(uint8_t cmd, int* ptr, int count)
    {
        write(cmd);
        for(int i = 0; i < count; i++) {
            write(ptr[i]);
        }
    }

    void update_part_offset(uint16_t offset)
    {
        if(loop_pos == 0) return; // no loop point
        uint16_t addr_pos = loop_addr_pos & 0xffff;
        uint16_t addr = (offset + loop_pos) & 0xffff;
        write_at(addr_pos, addr & 0xFF);
        write_at(addr_pos + 1, (addr >> 8) & 0xFF);
    }

    void end_part()
    {
        if (pos == 0) return; // no data (empty part
        if(loop_pos > 0) {
            write(CCD_JMP);
            loop_addr_pos = addr();           // needs relocation
            write16(static_cast<uint16_t>(loop_pos));      // also needs relocation
        } else {
            write(CCD_END);
        }
        size = pos;
    }

    // returns the current writing position as offset from `buffer`
    uint16_t addr() const { return static_cast<uint16_t>(pos); }
};

#endif // PART_BUFFER_H
