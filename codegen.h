#if !defined(CODEGEN_H)
#define CODEGEN_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "mucc_def.h"
#include "part_buffer.h"


class codegen
{
    part_buffer parts[MAXPART]; // A-Z plus some extra

    std::vector<part_buffer> drum_patterns; // drum patterns
    std::map<std::string, std::string> meta; // tag -> value

	int count_actual_parts() const
	{
    	int count = 0;
    	for(const auto& pb : parts) {
    		if(pb.pos > 0 && pb.length_written > 0) count++;
    	}
    	return count;
    }
	uint16_t write_meta(FILE* out_fp, const std::string& tag);
	void end_parts()
	{
		for (auto& pb : parts) {
			pb.end_part();
		}
	}

public:
    void register_meta(const std::string& tag, const std::string& value);
    auto& get_part(int index) { return parts[index]; }
	void add_drum_buffer(part_buffer& pb) { drum_patterns.push_back(pb); }
	int write_pmd(FILE* out_fp);

	void report_meta(FILE* fp);
};

#endif // !defined(CODEGEN_H)
