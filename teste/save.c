
// Test
int main()
{ 
    TEST_INIT_PARTINFO();
    char buffer[partitionInfo.BlockSize];
    char filename[] = "hello2.txt";
    //if (TEST_INIT_PARTINFO() != 0) return -1;
    SUPERBLOCK s;
    read_sector(0, (char*) &s); 
    printSuperblockInfo(&s);
	printf("Creating...\n");
    int i;
    /*
    for (i = '0'; i-'0' <= 64; ++i)
    {
        filename[5] = i;
        FILE2 handle;
        if ((handle = create2(filename)) == -1) printf("Didnt create file %s.\n", filename);
        close2(handle);
    }*/
    create2("/hello.txt");
    printf("Testing double dot.\n");
    create2("../helloDD.txt");
    create2("/../../helloDD2.txt");
    create2("./helloD.txt");
    /*
    int j;
    for(j = 0; j < 5; ++j)

        if (readNthBlock(&cwdDescriptor, j, buffer) == -1) break;
        for (i = 0; i < 16; ++i)
        {
            printf("----------- RECORD %d ------------\n", i);
            printRecordInfo((t2fs_record*) buffer + i);
        }
    }*/
    
    read_sector(0, (char*) &s); 
    printSuperblockInfo(&s);
    
    if (open2("hello.txt") < 0)
    {
        printf("Didn't find hello.txt\n");
    }
    if (open2("../../hello.txt") < 0)
    {
        printf("Didn't find ../../hello.txt\n");
    }
    if (open2(".././.././hello.txt") < 0)
        printf("Didn't find .././.././hello.txt\n");
    mkdir2("/hellodir");
    
    int j;
    t2fs_record createdDir;
    for (j = 0; j < 5; ++j)
    {
        if (readNthBlock(&cwdDescriptor, j, buffer) == -1) break;
        for (i = 0; i < 16; ++i)
        {
            printf("------------- RECORD %d --------------\n", i);
            printRecordInfo((t2fs_record*) buffer + i);
            if ( ((t2fs_record*)buffer+i)->TypeVal == TYPEVAL_DIR && i > 1)
                createdDir = *((t2fs_record*)buffer+i);
        }
    }
    for (j = 0; j < 5; ++j)
    {
        if (readNthBlock(&createdDir, j, buffer) == -1) break;
        for (i = 0; i < 16; ++i)
        {
            printf("------------- RECORD %d --------------\n", i);
            printRecordInfo((t2fs_record*) buffer + i);
        }
    }
    
    
    /*
    char str[1024];
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\"hellodir\");\n");
    chdir2("hellodir");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\".\");\n");
    chdir2(".");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\"..\");\n");
    chdir2("..");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\"..\");\n");
    chdir2("..");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    printf("chdir2(\".\");\n");
    chdir2(".");
    getcwd2(str, 1024);
    printf("CWD: %s\n", str);
    * */
    /*
    printf("Changing to hellodir...\n");
    chdir2("hellodir");
    printRecordInfo(&cwdDescriptor);
    printf("Opening \"..\" ...\n");
    DIR2 d = opendir2("..");
    DIRENT2 dentry;
    printf("Start reading...\n");
    while (readdir2(d, &dentry) == 0)
    {
        printf("Name: %s, Type: %d, Size (B): %lu\n", dentry.name, dentry.fileType, dentry.fileSize);
    }
    printf("Closing...\n");
    closedir2(d);
    
    chdir2("/");
    create2("./hellodir/test.txt");
    rmdir2("hellodir");
    delete2("../././././hello.txt");
    printf("Opening \"/\" ...\n");
    d = opendir2("/");
    printf("/ handle: %d\n", d);
    printf("Start reading...\n");
    while (readdir2(d, &dentry) == 0)
    {
        printf("Name: %s, Type: %d, Size (B): %lu\n", dentry.name, dentry.fileType, dentry.fileSize);
    }
    closedir2(d);
    */
    DIR2 d = opendir2("/");
    printf("Opening hello.txt...\n");
    FILE2 handle = open2("hello.txt");
    char buffer2[1024] = "Testando 123456789\nAlistinho na area\n";
    write2(handle, buffer2, strlen(buffer2));
    printf("Buffer2 after write: %s\n", buffer2);
    DIRENT2 dentry;
    while (readdir2(d, &dentry) == 0)
    {
        printf("Name: %s, Type: %d, Size (B): %lu\n", dentry.name, dentry.fileType, dentry.fileSize);
    }
    
    seek2(handle, 0);
    char buffer3[1024];
    read2(handle, buffer3, sizeof(buffer3));
    strcat(buffer3, "\0");
    printf("read: %s", buffer3);
    
    seek2(handle, 0);
    char a[1600];
    memset(a, 'A', sizeof(a));
    a[1599] = 'B';
    
    char r[2048];
    write2(handle, a, 1600);
    seek2(handle, 0);
    int howMany = read2(handle, r, sizeof(r));
    buffer3[howMany] = '\0';
    printf("read: %s", r);
    return 0;
}
