#pragma once

#include <cstdio>
#include <string>

class FFmpegPipe
{
public:
    FFmpegPipe() = default;
    ~FFmpegPipe()
    {
        Close();
    }

    bool Open(const std::string& command)
    {
#ifdef _WIN32
        pipe = _popen(command.c_str(), "wb");
#else
        pipe = popen(command.c_str(), "w");
#endif
        return pipe != nullptr;
    }

    void Write(const void* data, size_t size)
    {
        if (pipe)
            fwrite(data, 1, size, pipe);
    }

    void Close()
    {
        if (!pipe)
            return;

#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif

        pipe = nullptr;
    }

private:
    FILE* pipe = nullptr;
};