#if !defined(CODEGEN_H)
#define CODEGEN_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "mucc_def.h"

// c:/usr/piece/tools/mucc/reserved.h
enum CTRLCODE {
	CCD_END	=	0,
	CCD_LEX	=	127,
	CCD_RRR	=	0xE0+0,
	CCD_GAT	=	0xE0+1,
	CCD_JMP	=	0xE0+2,
	CCD_CAL	=	0xE0+3,
	CCD_RPT	=	0xE0+4,
	CCD_NXT	=	0xE0+5,
	CCD_TRS	=	0xE0+6,
	CCD_TMP	=	0xE0+7,
	CCD_INO	=	0xE0+8,
	CCD_VOL	=	0xE0+9,
	CCD_ENV	=	0xE0+10,
	CCD_DTN	=	0xE0+11,
	CCD_NOT	=	0xE0+12,
	CCD_PPA	=	0xE0+13,
	CCD_PON	=	0xE0+14,
	CCD_POF	=	0xE0+15,
	CCD_TAT	=	0xE0+16,
	CCD_VIB	=	0xE0+17,
	CCD_MVL	=	0xE0+18,
	CCD_MFD	=	0xE0+19,
	CCD_PFD	=	0xE0+20,
	CCD_BND	=	0xE0+21,
	CCD_BRK	=	0xF6,
	CCD_NOP	=	0xF7,
	CCD_LEG	=	0xF8,
	CCD_LOF	=	0xF9,
	CCD_EXP	=	0xFa,
	CCD_EXR	=	0xFb,
//	CCD_,			//
};

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


    // returns the current writing position as offset from `buffer`
    uint16_t addr() const { return static_cast<uint16_t>(pos); }
};


class codegen
{
    part_buffer parts[MAXPART]; // A-Z plus some extra

    std::vector<part_buffer> drum_patterns; // drum patterns
    std::map<std::string, std::string> meta; // tag -> value

	int count_actual_parts() const {
    	int count = 0;
    	for(const auto& pb : parts) {
    		if(pb.pos > 0 && pb.length_written > 0) count++;
    	}
    	return count;
    }
	uint16_t write_meta(FILE* out_fp, const std::string& tag);

public:
    void register_meta(const std::string& tag, const std::string& value);
    auto& get_part(int index) { return parts[index]; }
	void add_drum_buffer(part_buffer& pb) { drum_patterns.push_back(pb); }
	int write_pmd(FILE* out_fp);

	void report_meta(FILE* fp);
};



#endif // !defined(CODEGEN_H)
