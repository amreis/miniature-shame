// Esse teste serve para verificar criacao de subdiretorios.
// Criaremos tres niveis de subdiretorios e um arquivo em cada um
// Arvore final (caso executado sobre um disco limpo):
// --- /
// --- file1
// --- dir1
// --- --- file2
// --- --- dir2
// --- --- --- file3
// --- --- --- dir3
// Alem disso, a criacao eh feita usando tanto caminho absoluto quando chdir2 e
// caminhos relativos.

#include "t2fs.h"
#include <stdio.h>

int main()
{
	DIRENT2 dentry;
	DIR2 d;
	FILE2 f1 = create2("file1");
	mkdir2("/dir1");
	FILE2 f2 = create2("./dir1/file2");
	
	mkdir2("/././dir1/dir2");
	chdir2("/./dir1/dir2");
	FILE2 f3 = create2("./file3");
	chdir2("/");
	
	d = opendir2("/");

	
	printf("Reading /\n");
	while (readdir2(d, &dentry) == 0)
	{
		printf("Entry: Name => %s, Type => %s, Size => %lu.\n",
 			dentry.name,
 			(dentry.fileType == 0x01 ? "File" : "Directory"),
 			dentry.fileSize);
	}
	closedir2(d);
	
	chdir2("./dir1");
	d = opendir2(".");
	printf("Reading /dir1\n");
	while (readdir2(d, &dentry) == 0)
	{
		printf("Entry: Name => %s, Type => %s, Size => %lu.\n",
 			dentry.name,
 			(dentry.fileType == 0x01 ? "File" : "Directory"),
 			dentry.fileSize);
	}
	closedir2(d);
	
	chdir2("/dir1");
	mkdir2("./dir2/dir3");
	if ((d = opendir2("./dir2")) < 0) {
		printf("Error reading /dir1/dir2\n");
	}
	else
	{
		printf("Reading /dir1/dir2\n");
		while (readdir2(d, &dentry) == 0)
		{
			printf("Entry: Name => %s, Type => %s, Size => %lu.\n",
				dentry.name,
				(dentry.fileType == 0x01 ? "File" : "Directory"),
				dentry.fileSize);
		}
	}
	return 0;
}
