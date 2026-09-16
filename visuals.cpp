#include "visuals.hpp"
#include <iostream>
#include <sstream>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pwd.h>
#include <ctime>
#include <fstream>

#ifdef __APPLE__
#include <cstdio>
#include <array>
#endif


std::string build_prompt(const std::string &path, const std::string &branch, int exit_status){
    std::string label = path;
    if(!branch.empty()){
        std::string b = branch;
        while(!b.empty() && b.front() == ' ') b.erase(b.begin());
        while(!b.empty() && b.back() == ' ') b.pop_back();
        if(!b.empty() && b.front() == '[') b.erase(b.begin());
        if(!b.empty() && b.back() == ']') b.pop_back();
        label += " on " + b;
    }

    std::string prompt = "\n" + label + "   >   ";
    return prompt;
}

#ifdef __APPLE__
static std::string run_command(const std::string &cmd){
    std::array<char, 128> buffer;
    std::string result;
    FILE *pipe = popen(cmd.c_str(), "r");
    if(!pipe) return "unknown";
    while(fgets(buffer.data(), buffer.size(), pipe) != nullptr){
        result += buffer.data();
    }
    pclose(pipe);
    while(!result.empty() && (result.back() == '\n' || result.back() == ' '))
        result.pop_back();
    return result;
}
#endif

static std::string get_os(){
#ifdef __APPLE__
    return "macOS " + run_command("sw_vers -productVersion");
#else
    std::ifstream f("/etc/os-release");
    std::string line;
    while(std::getline(f, line)){
        if(line.rfind("PRETTY_NAME=", 0) == 0){
            std::string val = line.substr(12);
            if(!val.empty() && val.front() == '"') val = val.substr(1, val.size() - 2);
            return val;
        }
    }
    return "Unknown";
#endif
}

static std::string get_username(){
    struct passwd *pw = getpwuid(getuid());
    return pw ? std::string(pw->pw_name) : "unknown";
}

static std::string get_hostname(){
    char buf[256];
    if(gethostname(buf, sizeof(buf)) == 0) return std::string(buf);
    return "unknown";
}

static std::string get_kernel(){
    struct utsname u;
    if(uname(&u) == 0) return std::string(u.release);
    return "unknown";
}

static std::string get_arch(){
    struct utsname u;
    if(uname(&u) == 0) return std::string(u.machine);
    return "unknown";
}

static std::string get_uptime(){
#ifdef __APPLE__
    std::string secs = run_command("sysctl -n kern.boottime | awk -F'[ ,]' '{print $4}'");
    long boot_time = secs.empty() ? 0 : std::stol(secs);
    long now = time(nullptr);
    long uptime_secs = boot_time > 0 ? (now - boot_time) : 0;
    int hrs = (int)(uptime_secs / 3600);
    int mins = (int)((uptime_secs % 3600) / 60);
    return std::to_string(hrs) + "h " + std::to_string(mins) + "m";
#else
    std::ifstream f("/proc/uptime");
    double seconds = 0;
    if(f >> seconds){
        int hrs = (int)seconds / 3600;
        int mins = ((int)seconds % 3600) / 60;
        return std::to_string(hrs) + "h " + std::to_string(mins) + "m";
    }
    return "unknown";
#endif
}

static std::string get_cpu(){
#ifdef __APPLE__
    return run_command("sysctl -n machdep.cpu.brand_string");
#else
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while(std::getline(f, line)){
        if(line.rfind("model name", 0) == 0){
            size_t colon = line.find(':');
            if(colon != std::string::npos) return line.substr(colon + 2);
        }
    }
    return "unknown";
#endif
}

static std::string get_memory(){
#ifdef __APPLE__
    std::string total = run_command("sysctl -n hw.memsize");
    if(total.empty()) return "unknown";
    long total_bytes = std::stol(total);
    double total_gb = total_bytes / (1024.0 * 1024.0 * 1024.0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f GB", total_gb);
    return std::string(buf);
#else
    std::ifstream f("/proc/meminfo");
    std::string line;
    long total_kb = 0;
    while(std::getline(f, line)){
        if(line.rfind("MemTotal:", 0) == 0){
            sscanf(line.c_str(), "MemTotal: %ld kB", &total_kb);
            break;
        }
    }
    double total_gb = total_kb / (1024.0 * 1024.0);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f GB", total_gb);
    return std::string(buf);
#endif
}

static std::string get_terminal_size(){
    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0){
        return std::to_string(w.ws_col) + "x" + std::to_string(w.ws_row);
    }
    return "unknown";
}

static std::string get_datetime(){
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%a %d %b, %H:%M", localtime(&now));
    return std::string(buf);
}

static const char *get_ascii_art = R"ART(⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀                       ⢀⣤⠶⠒⠒⠦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀                  ⠀⡀⣬⠟⠁⠀⠀⠀⠀⠈⠳⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣀⣠⠤⠤⣄⣐⣁⠀⠀⢀⠀⢀⣀⣠⡤⠴⠾⠓⠒⠒⡒⠒⡒⠒⠒⠦⠽⢦⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⡠⢠⡞⠁⠀⠀⠀⠀⠀⠈⢙⣳⠶⠛⠉⠁⠀⠀⠀⠀⠀⠀⠀⣷⠀⡇⢀⡀⠀⠀⠀⠀⠈⠙⠲⢤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⣿⠀⠀⠀⠀⠀⠀⣠⠶⠋⠁⠀⠀⠀⠀⠀⣀⡀⠀⣠⣾⡿⠟⠀⠛⢻⣿⣄⠀⠀⢀⠀⠀⠀⠀⠉⠳⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⣿⠀⠀⠀⠀⢠⠞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠛⠿⠿⠿⠛⠁⠀⠀⠀⠀⠙⠿⠿⠿⠿⠃⠀⠀⠀⠀⠀⠈⢧⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⢻⠀⠀⠀⣰⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⢸⡄⠀⣰⠃⠀⠀⠀⠀⠀⠀⠀⣀⣠⣤⣤⣤⣤⣄⣀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣤⣶⣦⣤⣤⣀⠀⠀⠀⠀⢷⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⣇⢠⠇⠀⠀⠀⠀⢀⣤⣾⡿⠟⠛⠉⠉⠉⠙⠛⠿⣿⣦⡀⠀⠀⢀⣴⣿⠿⠛⠉⠉⠉⠉⠛⢿⣿⣦⠀⠀⠘⣇⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠸⣿⠀⠀⠀⠀⠰⣿⠟⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣿⡄⠀⣾⡿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠙⢿⣷⠀⠀⠙⠲⢤⡀⠀⠀⠀⠀
⠀⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⣀⣀⣀⣀⣈⣀⣀⣀⣀⣀⣀⣀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠳⡄⠀⠀
⠀⠀⠀⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡤⢶⠋⠉⠀⣸⠉⠙⠓⠦⢤⣄⣉⣛⣛⣻⣟⢋⣀⣠⣼⠋⠉⢹⠶⣤⡀⠀⠀⠀⠀⠀⠀⠙⡄⠀
⠀⠀⠀⢹⠀⠀⣀⠀⠀⠀⠀⠀⣠⠏⠀⠈⠳⣄⡴⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠀⠀⠀⠈⠳⠶⠋⠀⠈⠻⡄⠀⠀⠀⠀⠀⠀⢹⠀
⠀⠀⠀⠸⡟⠉⠉⠀⠀⠀⠀⠀⡏⠀⠀⠀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⠀⠀⠀⠀⠀⠀⢸⡇
⠀⠀⠀⠀⢻⡀⠀⢀⡀⠀⠀⠀⣧⠶⠛⠉⠉⠉⠉⠙⠓⠦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⠃⠀⠀⠀⠲⣦⣼⠇
⠀⠀⠀⠀⠀⠻⡶⠟⠋⠀⠀⠀⢳⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡾⠀⠀⠀⠀⠀⠀⡽⠀
⠀⠀⠀⠀⠀⠀⠙⢦⠀⠀⣠⣦⠀⠻⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠹⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡾⠁⠀⢠⣦⡀⠀⡼⠁⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠙⢿⣏⠀⠀⠀⠀⠙⠲⣤⣀⠀⠀⠀⠀⠀⠀⠀⢻⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠶⠋⠀⠀⠀⠀⠉⣻⠟⠁⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠙⠲⠦⢤⣄⣀⡀⠈⠙⠓⠒⠤⠤⠤⠤⣼⣤⡤⠤⠤⠤⠤⠖⠒⠚⠉⠀⠀⠀⢀⣀⣤⠶⠋⠁⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⢉⣹⣷⣶⣀⡀⠀⠀⠀⠀⣤⡤⠤⠤⠤⠤⠤⠴⣶⣶⠒⠛⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡤⠚⠉⡀⢈⣿⡉⠙⠛⠲⠦⣤⣿⡀⠀⠀⠀⣀⣠⡞⠁⣼⣤⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡴⠚⠁⢀⠄⣾⢷⡏⠉⠉⠓⠶⣤⣀⡾⠈⢷⡀⠀⠰⡇⣀⣙⣟⣩⡴⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⠁⢀⣤⣊⡴⠛⠁⡞⠀⠀⠀⠀⠀⠀⢩⡇⠀⠈⢷⣀⣴⣷⡈⢹⣏⣽⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢦⣾⣾⣿⣦⡀⢸⠃⠀⠀⠀⠀⠀⠀⡼⢻⡀⠀⠾⣿⣿⣿⡷⠚⠛⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣿⣿⣿⠿⡏⠀⠀⠀⠀⠀⠀⢰⠃⠀⢷⡀⠀⣹⠿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
)ART";


static std::vector<std::string> get_info_lines(){
    std::vector<std::string> info;
    info.push_back("User: " + get_username() + "@" + get_hostname());
    info.push_back("OS: " + get_os() + " (" + get_arch() + ")");
    info.push_back("Kernel: " + get_kernel());
    info.push_back("Uptime: " + get_uptime());
    info.push_back("CPU: " + get_cpu());
    info.push_back("Memory: " + get_memory());
    info.push_back("Terminal: " + get_terminal_size());
    info.push_back("Date: " + get_datetime());
    return info;
}

static size_t utf8_display_width(const std::string &s){
    size_t count = 0;
    for(size_t i = 0; i < s.size(); ){
        unsigned char c = s[i];
        if((c & 0x80) == 0) i += 1;
        else if((c & 0xE0) == 0xC0) i += 2;
        else if((c & 0xF0) == 0xE0) i += 3;
        else if((c & 0xF8) == 0xF0) i += 4;
        else i += 1;
        count++;
    }
    return count;
}

    void print_splash(){
    std::vector<std::string> art;
    std::istringstream art_stream(get_ascii_art);
    std::string art_line;
    while(std::getline(art_stream, art_line)) art.push_back(art_line);
    std::vector<std::string> info = get_info_lines();
    size_t art_width = 0;
    for(const auto &line : art) art_width = std::max(art_width, utf8_display_width(line));
    size_t info_width = 0;
    for(const auto &line : info) info_width = std::max(info_width, utf8_display_width(line));
    const size_t padding = 2;
    size_t box_inner_width = info_width + (padding * 2);

    std::vector<std::string> box_lines;
    {
        std::string top = "┌";
        for(size_t i = 0; i < box_inner_width; i++) top += "─";
        top += "┐";
        box_lines.push_back(top);
    }
    for(const auto &line : info){
        size_t line_width = utf8_display_width(line);
        size_t right_pad = box_inner_width - line_width - padding;
        std::string row = "│" + std::string(padding, ' ') + line + std::string(right_pad, ' ') + "│";
        box_lines.push_back(row);
    }
    {
        std::string bottom = "└";
        for(size_t i = 0; i < box_inner_width; i++) bottom += "─";
        bottom += "┘";
        box_lines.push_back(bottom);
    }

    size_t top_gap = (art.size() > box_lines.size()) ? (art.size() - box_lines.size()) / 2 : 0;
    std::vector<std::string> padded_box(top_gap, "");
    padded_box.insert(padded_box.end(), box_lines.begin(), box_lines.end());

    size_t max_lines = std::max(art.size(), padded_box.size());

    std::cout << "\n";
    for(size_t i = 0; i < max_lines; i++){
        std::string a_line = (i < art.size()) ? art[i] : "";
        std::string b_line = (i < padded_box.size()) ? padded_box[i] : "";

        size_t a_width = utf8_display_width(a_line);
        std::string a_pad = (a_width < art_width) ? std::string(art_width - a_width, ' ') : "";

        std::cout << a_line << a_pad << "  " << b_line << "\n";
    }
    std::cout << "\n";
}