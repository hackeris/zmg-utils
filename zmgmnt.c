//
// Created by rainm on 17-6-9.
//

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <memory.h>

#include "zmgfs.h"

#define DEBUG

#ifndef DEBUG
#   define log(format, ...)
#else
#   define log(format, ...) \
{ \
    FILE *FH = fopen( "/tmp/zmgmnt.log", "a" ); \
    if( FH ) { \
        fprintf( FH, "l. %4d: " format "\n", __LINE__, ##__VA_ARGS__ ); \
        fclose( FH ); \
    } \
}
#endif

static struct fuse_opt zmg_opts[] =
        {
                FUSE_OPT_END
        };

char *zmgfile = NULL;
char *mtpt = NULL;
int zmgfd = 0;
const char *zmgmap = NULL;

struct zmg_dir_entry *open_root(const char *zmgmap) {
    return (struct zmg_dir_entry *) (zmgmap + sizeof(struct zmg_header));
}

static int zmgfs_getattr(const char *path, struct stat *stbuf) {

    struct zmg_dir_entry *root = open_root(zmgmap);
    struct zmg_dir_entry *dentry = find_dir_entry_at(path, root);

    memset(stbuf, 0, sizeof(struct stat));
    if (dentry != NULL) {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }

    char filepath[256];
    strcpy(filepath, path);
    char *filename = (char *) last_name_of(filepath);
    filename[-1] = '\0';

    dentry = find_dir_entry_at(filepath, root);
    if (dentry != NULL) {
        struct zmg_file_entry *fentry = find_file_entry_with_filename_at(filename, dentry);
        if (fentry != NULL) {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = fentry->file_size;
            return 0;
        }
    }
    return -ENOENT;
}

static int zmgfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    log("readdir: %s\n", path);

    struct zmg_dir_entry *root = open_root(zmgmap);
    struct zmg_dir_entry *dentry = find_dir_entry_at(path, root);

    if (dentry == NULL) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    struct zmg_dir_entry *dentries = (struct zmg_dir_entry *) (((char *) dentry) + dentry->off_data);
    struct zmg_file_entry *fentries = (struct zmg_file_entry *) (dentries + dentry->n_dirs);

    for (int i = 0; i < dentry->n_dirs; i++) {
        filler(buf, dentries[i].name, NULL, 0);
    }

    for (int i = 0; i < dentry->n_files; i++) {
        filler(buf, fentries[i].name, NULL, 0);
        fentries = (struct zmg_file_entry *) (((char *) fentries + 1) + fentries->data_size);
    }
    return 0;
}

static int zmgfs_open(const char *path, struct fuse_file_info *fi) {

    struct zmg_dir_entry *root = open_root(zmgmap);
    struct zmg_file_entry *fentry = find_file_entry_at(path, root);

    if (fentry != NULL) {
        if ((fi->flags & 3) != O_RDONLY) {
            return -EACCES;
        }
        return 0;
    }
    return -ENOENT;
}

static int zmgfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {

    struct zmg_dir_entry *root = open_root(zmgmap);
    struct zmg_file_entry *fentry = find_file_entry_at(path, root);

    size_t filesize;
    if (fentry != NULL) {
        filesize = fentry->file_size;
        if (offset < filesize) {

            char *zipbuf = ((char *) fentry) + fentry->off_data;
            char *buffer = malloc(fentry->file_size);
            size_t sz;
            unzip_buffer_to_buffer(zipbuf, fentry->data_size, buffer, &sz);

            if (offset + size > filesize) {
                size = filesize - offset;
            }
            memcpy(buf, buffer + offset, size);
            free(buffer);
        } else {
            size = 0;
        }
    }
    return (int) size;
}

static int
zmg_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
    (void) data;

    switch (key) {
        case FUSE_OPT_KEY_OPT:
            return 1;

        case FUSE_OPT_KEY_NONOPT:
            if (!zmgfile) {
                zmgfile = strdup(arg);
                return 0;
            } else if (!mtpt) {
                mtpt = strdup(arg);
            }
            return 1;
        default:
            fprintf(stderr, "internal error\n");
            abort();
    }
}

static struct fuse_operations zmgfs_oper = {
        .getattr    = zmgfs_getattr,
        .readdir    = zmgfs_readdir,
        .open        = zmgfs_open,
        .read        = zmgfs_read,
};

int main(int argc, char *argv[]) {

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    if (fuse_opt_parse(&args, NULL, zmg_opts, zmg_opt_proc) == -1) {
        return -1;
    }

    printf("zmg file: %s\n", zmgfile);
    printf("mount point: %s\n", mtpt);

    zmgfd = open(argv[1], O_RDONLY);
    struct stat st;
    fstat(zmgfd, &st);
    size_t size = (size_t) st.st_size;
    zmgmap = mmap(NULL, size, PROT_READ, MAP_PRIVATE, zmgfd, 0);

    int ret = fuse_main(args.argc, args.argv, &zmgfs_oper, NULL);

    munmap((void *) zmgmap, size);
    close(zmgfd);

    return ret;
}
