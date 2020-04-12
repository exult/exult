
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <iostream>
#include <sstream>
#include <cassert>
#include "utils.h"

using std::cout;
using std::endl;
using std::istringstream;

const char *ss_data = "A BC DE FGHI JKLM NO\0";

int main() {
	istringstream iss(ss_data);

	uint8 outread1 = Read1(iss);
	cout << static_cast<char>(outread1) << endl;
	assert(static_cast<char>(outread1) == 'A');

	assert(static_cast<char>(Read1(iss)) == ' ');

	uint16 outread2 = Read2(iss);
	cout << static_cast<char>(outread2 & 0xff) << static_cast<char>((outread2 >> 8) & 0xff) << endl;
	assert(static_cast<char>(outread2 & 0xff) == 'B');
	assert(static_cast<char>((outread2 >> 8) & 0xff) == 'C');

	assert(static_cast<char>(Read1(iss)) == ' ');

	uint16 outread2high = Read2high(iss);
	cout << static_cast<char>((outread2high >> 8) & 0xff) << static_cast<char>(outread2high & 0xff) << endl;
	assert(static_cast<char>(outread2high & 0xff) == 'E');
	assert(static_cast<char>((outread2high >> 8) & 0xff) == 'D');

	assert(static_cast<char>(Read1(iss)) == ' ');

	uint32 outread4 = Read4(iss);
	cout << static_cast<char>(outread4 & 0xff) << static_cast<char>((outread4 >> 8) & 0xff)
	     << static_cast<char>((outread4 >> 16) & 0xff)  << static_cast<char>((outread4 >> 24) & 0xff) << endl;
	assert(static_cast<char>(outread4 & 0xff) == 'F');
	assert(static_cast<char>((outread4 >> 8) & 0xff) == 'G');
	assert(static_cast<char>((outread4 >> 16) & 0xff) == 'H');
	assert(static_cast<char>((outread4 >> 24) & 0xff) == 'I');

	assert(static_cast<char>(Read1(iss)) == ' ');

	uint32 outread4high = Read4high(iss);
	cout << static_cast<char>((outread4high >> 24) & 0xff) << static_cast<char>((outread4high >> 16) & 0xff)
	     << static_cast<char>((outread4high >> 8) & 0xff)  << static_cast<char>(outread4high & 0xff) << endl;
	assert(static_cast<char>(outread4high & 0xff) == 'M');
	assert(static_cast<char>((outread4high >> 8) & 0xff) == 'L');
	assert(static_cast<char>((outread4high >> 16) & 0xff) == 'K');
	assert(static_cast<char>((outread4high >> 24) & 0xff) == 'J');

	//speedtest();

	return 0;
}







