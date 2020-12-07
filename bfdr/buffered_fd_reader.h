#ifndef BUFFERED_FD_READER_H
#define BUFFERED_FD_READER_H

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
    bool eof;
    int lastErr;

public:
    BufferedFileDescriptorReader(size_t bufferSize) {
        bufferSize = alignSize(bufferSize, pageSize);
        buffer = (char*)mmap(NULL, bufferSize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
    }

    // Returns true if file was opened succesfully.
    bool open(const char *path, int additionalFlags = 0) {
        fd = ::open(path, O_RDONLY | O_DIRECT | additionalFlags);
        bufferDataSize = 0;
        bufferPos = 0;
        eof = false;
        lastErr = 0;
        return fd > 2;
    }

    ssize_t read(void *buf, size_t nbyte) {
        lastErr = 0;
        ssize_t bytesRead = 0;
        while (bufferDataSize - bufferPos <= nbyte - bytesRead) { // We do not have enough data in buffer to satisfy read, lets copy what we have and load more
            size_t br = bufferDataSize - bufferPos;
            memcpy(buf, buffer + bufferPos, br);
            bytesRead += br;
            buf += br;
            ssize_t ret = read(buffer, bufferSize);
            if (ret == 0) {
                eof = true;
                bufferPos = bufferDataSize;
                return bytesRead;
            } else if (ret < 0) {
                lastErr = errno;
                bufferPos = bufferDataSize;
                return bytesRead;
            }
            bufferDataSize = size_t(ret);
            bufferPos = 0;
        }
        memcpy(buf, buffer + bufferPos, nbyte - bytesRead);
        bufferPos += nbyte - bytesRead;
        return nbyte;
    }

    inline int lastErrno() {
        return lastErr;
    }

    int close() {
        int ret = ::close(fd);
        fd = -1;
        return ret;
    }

    ~BufferedFileDescriptorReader() {
        if (fd != -1) {
            ::close(fd);
        }
        munmap(buffer, bufferSize);
    }

private:

    static long pageSize;

    // Obtaining this required syscall so it's better to cache it than make it every time we create reader.
    static inline long getPageSize() {
        if (pageSize == 0) {
            pageSize = sysconf(_SC_PAGESIZE);
        }
        return pageSize;
    }
};

long BufferedFileDescriptorReader::pageSize = 0;

}

#endif // BUFFERED_FD_READER_H