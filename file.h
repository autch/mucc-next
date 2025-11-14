#if !defined(file_h)
#define file_h

#include <cstdio>
#include <string>
#include "mucc_def.h"

class file
{
    FILE* fp{nullptr};
    std::string_view view;
    std::string buffer;
    int line_number{0};

public:
    ~file() { close(); }

    int open(char* filename);
    void close();
    std::string_view readline();

    int get_line_number() const { return line_number; }
    const std::string& get_buffer() const { return buffer; }
    std::string_view get_p() const { return view; }
};

#endif // !defined(file_h)
