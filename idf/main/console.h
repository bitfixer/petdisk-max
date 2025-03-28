#pragma once

namespace Console {
    void init();
    void add_command(const char* cmd, const char* help, int(*func)(int,char**));
}