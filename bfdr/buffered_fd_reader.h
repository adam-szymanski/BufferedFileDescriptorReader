#ifndef BUFFERED_FD_READER_H
#define BUFFERED_FD_READER_H

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bfdr {

inline size_t alignSize(size_t s, size_t aligment) {
    return s + ((1 + (~s)) & (aligment - 1));
}

class BufferedFileDescriptorReader {
private:
    int fd = -1;
    char *buffer;
    size_t bufferSize;
    size_t bufferDataSize; // Amount of data loaded into buffer.
    size_t bufferPos; // Our position within buffer.
    off_t offsetRemainder;
    int lastError;

public:
    BufferedFileDescriptorReader(size_t bufferSize = (1 << 20))
        : bufferSize(bufferSize) {
        bufferSize = alignSize(bufferSize, getPageSize());
        buffer = (char*)mmap(NULL, bufferSize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
        if (buffer == (char*) -1) {
            printf("error when creating BufferedFileDescriptorReader: %s\n", strerror(errno));
            return;
        }
    }

    // Returns true if file was opened succesfully.
    bool open(const char *path, int additionalFlags = 0) {
        fd = ::open(path, O_RDONLY | O_DIRECT | additionalFlags);
        bufferDataSize = 0;
        bufferPos = 0;
        offsetRemainder = 0;
        lastError = 0;
        return fd > 2;
    }

    inline ssize_t read(void *buf, size_t nbyte) {
        ssize_t bytesRead = 0;
        while (bufferDataSize - bufferPos + bytesRead <= nbyte) { // We do not have enough data in buffer to satisfy read, lets copy what we have and load more
            size_t br = bufferDataSize - bufferPos;
            memcpy(buf, buffer + bufferPos, br);
            bytesRead += br;
            buf = (char*)buf + br;
            if (loadBuffer() <= 0) {
                return bytesRead;
            }
        }
        memcpy(buf, buffer + bufferPos, nbyte - bytesRead);
        bufferPos += nbyte - bytesRead;
        return nbyte;
    }

    inline int fstat(struct stat *statbuf) {
        return ::fstat(fd, statbuf);
    }

    inline int getChar(char& c) {
        if (bufferDataSize == bufferPos) {
            ssize_t ret = loadBuffer();
            if (ret <= 0) {
                return ret;
            }
        }
        c = buffer[bufferPos++];
        return 1;
    }

    int close() {
        int ret = ::close(fd);
        fd = -1;
        return ret;
    }

    inline off_t lseek(off_t offset, int whence) {
        // TODO let's check whether we are still in buffer, if yes then let's not clear all
        bufferDataSize = bufferPos = 0;
        offsetRemainder = offset & (getPageSize() - 1);
        offset -= offsetRemainder;
        return ::lseek(fd, offset, whence);
    }

    ~BufferedFileDescriptorReader() {
        if (fd != -1) {
            ::close(fd);
        }
        munmap(buffer, bufferSize);
    }

private:
    static long pageSize;
    static long pageSizeMask;

    // Obtaining this required syscall so it's better to cache it than make it every time we create reader.
    static inline long getPageSize() {
        if (pageSize == 0) {
            pageSize = sysconf(_SC_PAGESIZE);
        }
        return pageSize;
    }

    inline ssize_t loadBuffer() {
        ssize_t ret = ::read(fd, buffer, bufferSize);
        if (ret <= 0) {
            lastError = errno;
            bufferPos = bufferDataSize;
            return ret;
        }
        bufferDataSize = size_t(ret);
        bufferPos = offsetRemainder;
        offsetRemainder = 0;
        return ret;
    }
};

}

#endif // BUFFERED_FD_READER_H