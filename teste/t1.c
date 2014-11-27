#include "t2fs.h"
#include <stdio.h>
int main()
{
	FILE2 handle = create2("test_linking.txt");
	char buffer[] = "Test_writing\n";
	write2(handle, buffer, sizeof(buffer));
	seek2(handle, 0);
	char read[1024];
	read2(handle, read, sizeof(read));
	printf("Read from file...\n");
	printf("%s", read);
}
