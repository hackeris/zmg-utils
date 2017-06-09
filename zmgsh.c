//
// Created by rainm on 17-6-9.
//

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int zmgsh_main(int argc, char **argv) {

    int fd = open(argv[1], O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    size_t size = (size_t) st.st_size;
    const char *buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);



    munmap((void *) buf, size);
    close(fd);
    return 0;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Usage: %s <zmg file>\n", argv[0]);
        exit(1);
    }

    return zmgsh_main(argc, argv);
}