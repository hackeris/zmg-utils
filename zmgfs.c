//
// Created by rainm on 17-6-9.
//

#include <string.h>
#include <stdio.h>
#include <zlib.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "zmgfs.h"

void init_zmg_header(struct zmg_header *header) {
    strcpy(header->magic, "zmg");
    header->version = ZMG_VERSION;
}

int zip_buffer_to_file(const char *buffer, size_t size, FILE *dest, size_t *outsize, int level) {

    *outsize = 0;

    int ret, flush;
    unsigned have;
    z_stream strm;
    unsigned char out[CHUNK_SIZE] = {0};

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, level);
    if (ret != Z_OK)
        return ret;

    size_t pos = 0;

    /* compress until end of file */
    do {
        int blk = (int) ((size - pos) > CHUNK_SIZE ? CHUNK_SIZE : (size - pos));
        if (blk == 0) {
            break;
        }
        strm.avail_in = (uInt) blk;
        flush = pos + blk < size ? Z_NO_FLUSH : Z_FINISH;
        strm.next_in = (Bytef *) (buffer + pos);

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK_SIZE - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void) deflateEnd(&strm);
                return Z_ERRNO;
            }
            *outsize += have;
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        pos += blk;
        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void) deflateEnd(&strm);
    return Z_OK;
}

int unzip_buffer_to_file(const char *buffer, size_t size, FILE *dest) {

    int ret;
    unsigned have;
    z_stream strm;
    unsigned char out[CHUNK_SIZE];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return ret;

    size_t pos = 0;
    /* decompress until deflate stream ends or end of file */
    do {
        int blk = (int) ((size - pos) > CHUNK_SIZE ? CHUNK_SIZE : (size - pos));
        if (blk == 0) {
            break;
        }
        strm.avail_in = (uInt) blk;
        strm.next_in = (Bytef *) (buffer + pos);

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
                case Z_NEED_DICT:
                    ret = Z_DATA_ERROR;     /* and fall through */
                case Z_DATA_ERROR:
                case Z_MEM_ERROR:
                    (void) inflateEnd(&strm);
                    return ret;
            }
            have = CHUNK_SIZE - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void) inflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        pos += blk;

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void) inflateEnd(&strm);
    return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

int zip_file_to_file(const char *filename, FILE *dest, size_t *outsize) {

    int _fd = open(filename, O_RDONLY);

    struct stat st;
    fstat(_fd, &st);
    size_t _size = (size_t) st.st_size;
    char *_buf = mmap(NULL, _size, PROT_READ, MAP_PRIVATE, _fd, 0);

    int ret = zip_buffer_to_file(_buf, _size, dest, outsize, Z_DEFAULT_COMPRESSION);

    munmap(_buf, _size);

    close(_fd);
    return ret;
}
