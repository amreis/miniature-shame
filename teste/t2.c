// Esse teste faz a criacao de varios arquivos, usando caminhos absolutos e 
// relativos, mas tambem utilizando .. e . no caminho para testar se estamos 
// subindo e descendo corretamente na arvore de diretorios

#include "t2fs.h"
#include <stdio.h>

int main()
{
	// Criar arquivos na raiz
	FILE2 files[4];
	files[0] = create2("/hello.txt");
    printf("Testing double dot.\n");
    files[1] = create2("../helloDD.txt");
    files[2] = create2("/../../helloDD2.txt");
    files[3] = create2("./helloD.txt");
    
    DIRENT2 dentry;
    DIR2 d = opendir2("/");
 	
 	while (readdir2(d, &dentry) == 0)
 	{
 		printf("Entry: Name => %s, Type => %s, Size => %lu.\n",
 			dentry.name,
 			(dentry.fileType == 0x01 ? "File" : "Directory"),
 			dentry.fileSize);
 	}
 	   
    int i;
    for (i = 0; i < 4; ++i)
    	close2(files[i]);
	return 0;
}
