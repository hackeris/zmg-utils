//
// Created by rainm on 17-6-9.
//

#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <memory.h>
#include "zmgfs.h"

int ls_dentry(struct zmg_dir_entry *dentry) {

    struct zmg_dir_entry *entry = (struct zmg_dir_entry *) (((char *) dentry) + dentry->off_data);
    struct zmg_file_entry *fentry = (struct zmg_file_entry *) (entry + dentry->n_dirs);

    for (int i = 0; i < dentry->n_dirs; i++) {
        printf("%s\t<DIR>\n", entry[i].name);
    }

    for (int i = 0; i < dentry->n_files; i++) {
        printf("%s\n", fentry->name);
        fentry = (struct zmg_file_entry *) (((char *) fentry + 1) + fentry->data_size);
    }
    return 0;
}

int ls_dir(const char *path, const char *buff) {

    struct zmg_dir_entry *root = (struct zmg_dir_entry *) (buff + sizeof(struct zmg_header));
    struct zmg_dir_entry *dentry = find_dir_entry_at(path, root);

    ls_dentry(dentry);

    return 0;
}

int cat_file(const char *path, const char *buff) {

    char filepath[256];
    strcpy(filepath, path);

    char *filename = (char *) last_name_of(filepath);
    filename[-1] = '\0';

    struct zmg_dir_entry *root = (struct zmg_dir_entry *) (buff + sizeof(struct zmg_header));
    struct zmg_dir_entry *dentry = find_dir_entry_at(filepath, root);

    struct zmg_file_entry *fentry = find_file_entry_with_filename_at(filename, dentry);

    char *zipbuf = ((char *) fentry) + fentry->off_data;

    char *buf = malloc(fentry->file_size + 1);
    memset(buf, 0, fentry->file_size + 1);
    size_t sz = fentry->file_size;
    unzip_buffer_to_buffer(zipbuf, fentry->data_size, buf, &sz);
    printf("%s", buf);

    return 0;
}

int zmgsh_main(int argc, char **argv) {

    int fd = open(argv[1], O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    size_t size = (size_t) st.st_size;
    const char *buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

    char pwd[256];
    strcpy(pwd, "");

    char cmd[256];
    while (1) {
        printf("$ ");
        fgets(cmd, sizeof(cmd), stdin);
        cmd[strlen(cmd) - 1] = '\0';

        if (strcmp(cmd, "ls") == 0) {
            ls_dir(pwd, buf);
        } else if (strcmp(cmd, "exit") == 0) {
            break;
        } else if (strstr(cmd, "cd") != NULL) {
            char *pdir = &cmd[strlen(cmd) - 1];
            while (*pdir != ' ') { pdir--; }
            pdir++;

            strcat(pwd, "/");
            strcat(pwd, pdir);
        } else if (strstr(cmd, "cat") != NULL) {
            char *pfile = &cmd[strlen(cmd) - 1];
            while (*pfile != ' ') { pfile--; }
            pfile++;

            char filename[256];
            strcpy(filename, pwd);
            strcat(filename, "/");
            strcat(filename, pfile);

            cat_file(filename, buf);
        }
    }

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
