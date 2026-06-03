#pragma once

#include <cstdio>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

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
        if (command.empty()) return false;
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &command[0], (int)command.size(), NULL, 0);
        std::wstring wcommand(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, &command[0], (int)command.size(), &wcommand[0], sizeNeeded);

        pipe = _wpopen(wcommand.c_str(), L"wb");
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