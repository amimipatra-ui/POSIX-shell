#pragma once
#include <string>
#include <vector>

extern const std::string RESET;
extern const std::string BOLD;
extern const std::string BLUE;
extern const std::string GREEN;
extern const std::string YELLOW;
extern const std::string RED;
extern const std::string CYAN;
extern const std::string BR_BLUE;
extern const std::string BR_GREEN;

std::string colorize(const std::string &color, const std::string &text);
std::string build_prompt(const std::string &path, const std::string &branch, int exit_status);
void print_splash();