#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

// Writes the fuzz input to a mkstemp() file under $TMPDIR (default /tmp) and removes it on scope exit.
struct ScratchFile {
    std::string path;
    bool        ok = false;
    ScratchFile(const uint8_t *data, size_t size, const char *suffix = "")
    {
        const char *dir = getenv("TMPDIR");
        std::string tmpl = std::string(dir && *dir ? dir : "/tmp") + "/bbs_fuzz_XXXXXX";
        std::string buf  = tmpl;
        int         fd   = mkstemp(&buf[0]);
        if (fd < 0)
            return;
        size_t off = 0;
        while (off < size) {
            ssize_t n = write(fd, data + off, size - off);
            if (n <= 0)
                break;
            off += size_t(n);
        }
        close(fd);
        path = buf;
        ok   = off == size;
    }
    ~ScratchFile()
    {
        if (!path.empty())
            unlink(path.c_str());
    }
};
