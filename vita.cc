#ifdef VITA

#include <string.h>
#include <stdlib.h>

#include <SDL.h>
#include <psp2/system_param.h>
#include <psp2/apputil.h>


char *strdup(const char *s) {
	int len;
	int i1;
	char *z2;

	len=strlen(s);
	len++;

	char *dup = static_cast<char *>(malloc(static_cast<size_t>(len)));
	if(dup != NULL) {
		z2=dup;
		i1=0;
		while(i1 < static_cast<int>(strlen(s))) {
			*z2=s[i1];	
			z2++;
		}
		*z2=0x0;	
	}	
		
	return(dup);
}

int isascii (int c) {
        if(c >= 0 && c <= 127) 
                return(1);
        else
                return(0);
}

#endif
