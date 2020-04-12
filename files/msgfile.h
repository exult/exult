/**
 ** Msgfile.h - Read in text message file.
 **
 ** Written: 6/25/03
 **/

#ifndef INCL_MSGFILE_H
#define INCL_MSGFILE_H 1

#include <iosfwd>
#include <string>
#include <vector>

class IDataSource;

int Read_text_msg_file(
    IDataSource *in,
    std::vector<std::string> &strings,    // Strings returned here, each
    //   allocated on heap.
    const char *section = nullptr
);
int Read_text_msg_file(
    std::istream &in,
    std::vector<std::string> &strings,    // Strings returned here, each
    //   allocated on heap.
    const char *section = nullptr
);
bool Search_text_msg_section(
    IDataSource *in,
    const char *section = nullptr
);
int Read_text_msg_file_sections(
    IDataSource *in,
    std::vector<std::vector<std::string> > &strings,   // Strings returned here
    const char *sections[],         // Section names
    int numsections
);
int Read_text_msg_file_sections(
    std::istream &in,
    std::vector<std::vector<std::string> > &strings,   // Strings returned here
    const char *sections[],         // Section names
    int numsections
);
void Write_msg_file_section(
    std::ostream &out,
    const char *section,
    std::vector<std::string> &items
);

#endif
