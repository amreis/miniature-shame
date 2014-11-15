#include "apidisk.h"
#include "t2fs.h"

// TODO - GLOBAL ARRAY OF OPEN FILES (AT MOST 20)

int strncpy2(char *dst, const char *org, int n)
{
	int i;
	for (i = 0; i < n; ++i)
	{
		if (org[i] == STR_END)
		{
			dst[i] = STR_END;
			break;
		}
		dst[i] = org[i];
	}
	return 0;
}

int identify2(char *name, int size)
{
	char id[] = "Alister M. - 220494, Eduardo V. - 218313";
	if (size < sizeof(id))
		return -1;
	else strncpy2(name, id, size);
	return 0;
}
