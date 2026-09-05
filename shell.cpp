#include <cctype>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <filesystem>
#include <sys/wait.h>
#include <system_error>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <glob.h>
#include <unordered_map>

std::unordered_map<std::string, std::string> aliases;

namespace fs = std::filesystem;
int last_exit_status = 0;
volatile sig_atomic_t get_sigint = 0;

std::string fetch_path(){
    fs::path current_path = fs::current_path();
    fs::path chopped_path;
    int count = 0;
    for(const auto &path: current_path){
        if(!path.empty() && path != "/"){
            chopped_path /= path;
            count++;
            if(count == 2) break;
        }
    }
    return chopped_path.string();
}

std::string fetch_branch(){
    fs::path current_path = fs::current_path();
    while(true){
        fs::path git_path = current_path / ".git" / "HEAD";
        if(fs::exists(git_path)){
            std::ifstream head_file(git_path);
            if(head_file.is_open()){
                std::string line;
                if(std::getline(head_file,line)){
                    while(!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')){
                        line.pop_back();
                    }
                    if(line.rfind("ref: refs/heads/",0) == 0){
                        return " [" + line.substr(16) + "] ";
                    }
                    return " [" + line.substr(0,7) + "] ";
                }
            }
        }
        if(current_path == current_path.root_path() || current_path.empty())
            break;
        current_path = current_path.parent_path();
    }
    return std::string();
}

std::vector<std::string> tokenizer(const std::string &command){
    std::vector<std::string> args;
    std::string current;
    bool in_token = false;
    char quote_char = '\0';

    for(size_t i = 0; i < command.size(); i++){
        char c = command[i];

        if(quote_char != '\0'){
            if(c == quote_char){
                quote_char = '\0';
            }
            else{
                current += c;
            }
            continue;
        }

        if(c == '"' || c == '\''){
            quote_char = c;
            in_token = true;
            continue;
        }

        if(c == '\\' && i + 1 < command.size()){
            current += command[i + 1];
            i++;
            in_token = true;
            continue;
        }

        if(std::isspace(static_cast<unsigned char>(c))){
            if(in_token){
                args.push_back(current);
                current.clear();
                in_token = false;
            }
            continue;
        }
        current += c;
        in_token = true;
    }
    if(in_token) {args.push_back(current);}
    return args;
}

void cmd_cd(const std::vector<std::string> &args){
    if(args.size() < 2){
        const char *home = getenv("HOME");
        if(!home) {std::cerr << "home not found !!! \n";}
        std::error_code ec;
        fs::current_path(home,ec);
        if(ec) {std::cerr << "cd: " << ec.message() << "not found \n";}
        return;
    }
    fs::path target = args[1];
    std::error_code ec;
    fs::current_path(target,ec);
    if(ec) {std::cerr << "cd: " << ec.message() << "not found \n";}
}

struct Redirect{
    std::string out_file;
    bool append = false;
    std::string in_file;
};

Redirect parse_redirects(std::vector<std::string> &args){
    Redirect r;
    std::vector<std::string> clean;
    for(int i = 0; i < args.size(); i++){
        if(args[i] == ">" && i + 1 < args.size()){
            r.out_file = args[++i];
        }
        else if(args[i] == ">>" && i + 1 < args.size()){
            r.out_file = args[++i];
            r.append = true;
        }
        else if(args[i] == "<" && i + 1< args.size()){
            r.in_file = args[++i];
        }
        else{clean.push_back(args[i]);}
    }
    args = clean;
    return r;
}

void apply_redirects(const Redirect r){
    if(!r.out_file.empty()){
        int flags = O_WRONLY | O_CREAT | (r.append ? O_APPEND : O_TRUNC);
        int fd = open(r.out_file.c_str(), flags, 0644);
        if(fd < 0) {std::cerr << "faied to open"; _exit(1);}
        dup2(fd, 1);
        close(fd); 
    }
    if(!r.in_file.empty()){
        int fd = open(r.in_file.c_str(), O_RDONLY);
        if(fd < 0) {std::cerr << "failed to open";_exit(1);}
        dup2(fd, 0);
        close(fd);
    }
}

std::vector<std::vector<std::string>> parse_pipeline(const std::vector<std::string> &args){
    std::vector<std::vector<std::string>> commands;
    std::vector<std::string> current;
    for(const auto &tok: args){
        if(tok == "|"){
            if(current.empty()){
                std::cerr << "syntax error near unexpected token '|'\n";
                return {};
                }
                commands.push_back(current);
                current.clear();
                continue;
            }else{
            current.push_back(tok);
            }
        }
    if(!current.empty())
        commands.push_back(current);
    return commands;
}

void run_pipeline(std::vector<std::vector<std::string>> &commands){
    int num = commands.size();
    int prev_read = -1;
    int fd[2];
    
    std::vector<pid_t> pids;
    pid_t pgid = 0;

    for(int i = 0; i < num; i++){
        pipe(fd);

        pid_t pid = fork();

        if(pid < 0) {std::cerr << "fork failed";}

        if(pid == 0){
            if(pgid == 0) setpgid(0, 0);
            else setpgid(0, pgid);
            signal(SIGINT, SIG_DFL);   
            signal(SIGTSTP, SIG_DFL);
            if(prev_read != -1){
                dup2(prev_read, 0);
                close(prev_read);
            }
            if(i < num - 1){
                dup2(fd[1], 1);
            }
            close(fd[0]);
            close(fd[1]);

            Redirect r = parse_redirects(commands[i]);
            apply_redirects(r);
            
            std::vector<char *> argv;
            for(const auto &a: commands[i]){
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            std::cerr<< "command not found \n";
            _exit(127);
        }
        else{
            if(pgid == 0) pgid = pid;
            setpgid(pid, pgid);

            pids.push_back(pid);
            if(prev_read != -1) 
                    close(prev_read);
                close(fd[1]);
                prev_read = fd[0];
            }
        }

        if(prev_read != -1) close(prev_read);
        tcsetpgrp(STDIN_FILENO, pgid);

        for(size_t i = 0; i < pids.size(); i++){
        int status;
        waitpid(pids[i], &status, 0);
        if(i == pids.size() - 1){
        if(WIFEXITED(status)){
            last_exit_status = WEXITSTATUS(status);
        }
        else if(WIFSIGNALED(status)){
            last_exit_status = 128 + WTERMSIG(status);
        }
    }
}
tcsetpgrp(STDIN_FILENO, getpid());
}

    void handle_signal(int sig){
        // write(STDOUT_FILENO, "[handler fired]\n", 17);  
            get_sigint = 1;
    }

    int check_sigint(){
    if(get_sigint){ 
        get_sigint = 0; 
        std::cout << "\n";
        std::cout.flush();
        rl_free_line_state();
        rl_cleanup_after_signal(); 
        rl_on_new_line(); 
        rl_replace_line("", 0); 
        rl_redisplay(); 
    }
    return 0;
}

struct Job{
    pid_t pid;
    std::string command;
    bool stopped;
};
std::vector<Job> jobs;

void cmd_jobs(){
    for(size_t i = 0; i < jobs.size(); i++){
    std::cout << "[" << i + 1 << "]  ±  "
        << (jobs[i].stopped ? "suspended" : "running") << "  "
        << jobs[i].command << "\n";
    }
}

void cmd_fg(const std::vector<std::string> &args){
    if(jobs.empty()) {std::cerr << "fg: no current jobs \n"; return;}

    int idx = jobs.size() - 1;
    if(args.size() >= 2) idx = std::stoi(args[1]) - 1;
    if(idx < 0 || idx >= (int)jobs.size()) {std::cerr << "fg: no such jobs \n"; return;}

    Job j = jobs[idx];
    jobs.erase(jobs.begin() + idx);

    std::cout << "[" << idx + 1 << "]  ±  " << j.pid << "  continued  " << j.command << "\n";
    tcsetpgrp(STDIN_FILENO, j.pid);
    kill(j.pid, SIGCONT);

    int status;
    waitpid(j.pid,&status,WUNTRACED);
    tcsetpgrp(STDIN_FILENO, getpid());
    if(WIFSTOPPED(status)){
    jobs.push_back({j.pid, j.command, true});
    std::cout << "\n[" << idx + 1 << "]  ±  " << j.pid << "  suspended  " << j.command << "\n";
    }

}
void cmd_bg(const std::vector<std::string> &args){
    if(jobs.empty()) {std::cerr << "bg: no such jobs \n"; return;}

    int idx = jobs.size() - 1;
    if(args.size() >= 2) idx = std::stoi(args[1]) - 1;
    if(idx < 0 || idx >= (int)jobs.size()) {std::cerr << "bg: no such jobs \n"; return;}

    jobs[idx].stopped = false;
    kill(jobs[idx].pid, SIGCONT);

    std::cout << "[" << idx + 1 << "]  ±  " << jobs[idx].pid << "  continued  " << jobs[idx].command << "\n";
}

std::vector<std::string> get_executables(const std::string &prefix){
    std::vector<std::string> matches;
    const char *path_exec = getenv("PATH");
    if(!path_exec) return matches;

    std::stringstream ss(path_exec);
    std::string dir;
    while(std::getline(ss,dir,':')){
        std::error_code ec;
        if(!fs::exists(dir,ec) || !fs::is_directory(dir,ec)) continue;;
        for(const auto &entry: fs::directory_iterator(dir,ec)){
            if(ec) break;
            std::string name = entry.path().filename().string();
            if(name.compare(0,prefix.size(),prefix) == 0){
                matches.push_back(name);
            }
        }
    }
    return matches;
}

char *command_generator(const char *text, int state){
    static std::vector<std::string> matches;
    static size_t idx;

    if(state == 0){
        idx = 0;
        matches.clear();
        std::string prefix(text);
    
        static std::vector<std::string> builtins = {"cd", "jobs", "fg", "bg", "exit", "quit"};
        for(const auto &b: builtins){
            if(b.compare(0,prefix.size(),prefix) == 0)
            matches.push_back(b);
        }
        auto path_matches = get_executables(prefix);
        matches.insert(matches.end(), path_matches.begin(), path_matches.end());
    }

    if(idx < matches.size()){
        return strdup(matches[idx++].c_str());
    }
    return nullptr;
}

char **shell_completion(const char *text, int start, int end){
    if(start == 0){
        return rl_completion_matches(text, command_generator);
    }
    return nullptr;
}

void display_matches_with_gap(char **matches, int num_matches, int max_length){
    rl_display_match_list(matches, num_matches, max_length);   
    std::cout << "\n";                                         
    rl_forced_update_display();                                
}

std::string expand_env(const std::string &token){
    if(token.empty() || token[0] != '$') return token;

    if(token == "$?"){
        return std::to_string(last_exit_status);
    }

    std::string var = token.substr(1);
    const char *val = getenv(var.c_str());
    return val ? std::string(val) : "";
}

std::vector<std::string> expand_glob(const std::string &token){
    if(token.find('*') == std::string::npos && token.find('?') == std::string::npos){
        return {token};
    }
    glob_t glob_result;
    int ret = glob(token.c_str(), GLOB_NOCHECK, nullptr, &glob_result);

    std::vector<std::string> matches;
    if(ret == 0){
    for(size_t i = 0; i < glob_result.gl_pathc; i++){
        matches.push_back(glob_result.gl_pathv[i]);
    }
    }
    else{
        matches.push_back(token);
    }
    globfree(&glob_result);
    return matches;
}

void cmd_alias(const std::vector<std::string> &args){
    if(args.size() < 2){
        for(const auto &[k,v]: aliases)
        std::cout << "alias " << k << "=" << v << "\n";
    return;
    }

    std::string joined = args[1];
    for(size_t i = 2; i < args.size(); i++) joined += " " + args[i];
    
    size_t eq = joined.find('=');
    if(eq == std::string::npos){
        auto it = aliases.find(joined);
        if(it != aliases.end())
            std::cout << "alias " << it->first << "=" << it->second << "'\n" << std::endl;
        else
            std::cerr << "allias: " << joined << " not found\n" << std::endl;
        return;
    }
    std::string name = joined.substr(0,eq);
    std::string value = joined.substr(eq + 1);
    aliases[name] = value;
}

void cmd_unaliase(const std::vector<std::string> &args){
    if(args.size() < 2) {std::cerr << "unalias usgae: unalias name \n"; return;}
    aliases.erase(args[1]);
}

void expand_alias(std::vector<std::string> &args){
    if(args.empty()) return;
    auto it = aliases.find(args[0]);
    if(it == aliases.end()) return;

    std::vector<std::string> alias_token = tokenizer(it -> second);
    std::vector<std::string> new_args = alias_token;
    new_args.insert(new_args.end(), args.begin() + 1, args.end());
    args = new_args;
}

struct ChainSegment{
    std::vector<std::string> tokens;
    std::string op;
};

std::vector<ChainSegment> parse_chain(const std::vector<std::string> &args){
    std::vector<ChainSegment> segments;
    std::vector<std::string> current;

    for(size_t i = 0; i < args.size(); i++){
        const std::string &tok = args[i];
        if(tok == "&&" || tok == "||" || tok == ";"){
            if(current.empty()){
                std::cerr << "syntax error near unexpected token " << tok << "\n";
                return {};
            }
            segments.push_back({current, tok});
            current.clear();
        }
        else{
            current.push_back(tok);
        }
    }
    if(!current.empty()){
        segments.push_back({current, ""});
    }
    else if(!segments.empty()){
        std::cerr << "syntax error: trailing operator \n";
        return {};
    }
    return segments;
}

void run_external(std::vector<std::string> &args){
    bool bg = false;
    if(!args.empty() && args.back() == "&"){
        bg = true;
        args.pop_back();
    }
    Redirect r = parse_redirects(args);
    std::vector<char *> argv;
    for(const auto &a: args){
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();

    if(pid < 0) {std::cerr << "fork failed \n"; return; }

    if(pid == 0){
        setpgid(0, 0);
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        apply_redirects(r);
        execvp(argv[0], argv.data());
        std::cerr << "command not found \n";
        _exit(127);
    }
    setpgid(pid, pid);  
    if(bg){
        jobs.push_back({pid,args[0],false});
        std::cout << "[" << jobs.size() << "]  ±  " << pid << "\n";
        return;
    }
    tcsetpgrp(STDIN_FILENO, pid); 

    int status;
    waitpid(pid, &status, WUNTRACED); 
    tcsetpgrp(STDIN_FILENO, getpid()); 

    if(WIFEXITED(status)){
        last_exit_status = WEXITSTATUS(status);
    }

    if(WIFSTOPPED(status)){  
        jobs.push_back({pid, args[0], true});
        std::cout << "\n[" << jobs.size() << "]  ±  " << pid << "  suspended  " << args[0] << "\n";
    }
    else if(WIFSIGNALED(status)){
        last_exit_status = 128 + WTERMSIG(status);
    }
}

int main(){
    struct sigaction sa;  
    sa.sa_handler = handle_signal;  
    sigemptyset(&sa.sa_mask);  
    sa.sa_flags = 0;     

    sigaction(SIGINT, &sa, nullptr);

    rl_catch_signals = 0; 
    rl_event_hook = check_sigint; 

    pid_t shell_gpid = getpid(); 
    setpgid(shell_gpid, shell_gpid); 

    signal(SIGTTOU, SIG_IGN);   
    signal(SIGTTIN, SIG_IGN); 
    signal(SIGTSTP, SIG_IGN);

    tcsetpgrp(STDIN_FILENO, shell_gpid);
    std::string command;
    rl_attempted_completion_function = shell_completion;
    rl_variable_bind("show-all-if-ambiguous", "on"); 
    rl_completion_query_items = -1;// dont ask 
    rl_completion_display_matches_hook = display_matches_with_gap;  
    while(command != "quit" && command != "exit"){
        for(auto it = jobs.begin(); it !=jobs.end();){
            int status;
            pid_t res = waitpid(it->pid, &status, WNOHANG); 
            if(res > 0){
                int job_num = std::distance(jobs.begin(), it) + 1;
            std::cout << "[" << job_num << "]  ±  " << it->pid << "  done  " << it->command << "\n";
            it = jobs.erase(it);
            }else{
                ++it;
            }
        }
        std::string prompt = fetch_path() + fetch_branch() + " >  ";
        char *line = readline(prompt.c_str());
        if(!line){
            std::cout << "\n";
            break;
        }
        command = line;
        if(!command.empty())
            add_history(line);
        free(line);
        if(command.empty()) continue;
        std::vector<std::string> args = tokenizer(command);
        if(args.empty()) continue;
        expand_alias(args);
        for(auto &a: args){
            a = expand_env(a);
        }
        std::vector<std::string> expanded_args;
        for(const auto &a : args){
        auto matches = expand_glob(a);
        expanded_args.insert(expanded_args.end(), matches.begin(), matches.end());
        }
        args = expanded_args;

        if(args[0] == "exit" || args[0] == "quit") break;
        if(args[0] == "cd"){
            cmd_cd(args);
            continue;
        }
        if(args[0] == "jobs"){cmd_jobs(); continue; }
        if(args[0] == "fg"){cmd_fg(args); continue; }
        if(args[0] == "bg"){cmd_bg(args); continue;}
        if(args[0] == "alias"){cmd_alias(args);continue;}
        if(args[0] == "unalias"){cmd_unaliase(args); continue;}
        std::vector<ChainSegment> chain = parse_chain(args);
        bool should_run = true;

        for(const auto &seg : chain){
        if(!should_run){
        should_run = true;
        continue;
    }
    std::vector<std::vector<std::string>> commands = parse_pipeline(seg.tokens);
    if(commands.empty()) continue;

    std::vector<std::string> seg_args = seg.tokens;
    if(commands.size() == 1){run_external(seg_args);}
    else{ run_pipeline(commands);}
    if(seg.op == "&&" && last_exit_status != 0){ should_run = false;}
    else if(seg.op == "||" && last_exit_status == 0){ should_run = false;}
}
    std::cout << std::endl;
    }
}
