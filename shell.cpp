#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <signal.h>
#include <termios.h>
#include <readline/readline.h>
#include <readline/history.h>

namespace fs = std::filesystem;

void handle_sigint(int sig);

std::string fetch_username(){
    fs::path curr_path = fs::current_path();
    fs::path chopped_path;
    int count = 0;
    for(const auto &path: curr_path){
        if(!path.empty() && path != "/"){
            chopped_path /= path;
            count++;
            if(count == 2)
            break;
        }
    }
    return chopped_path;
}

std::string fetch_branch(){
    fs::path curr_path = fs::current_path();
    while(true){
        fs::path git_path = curr_path / ".git" / "HEAD";
        if(fs::exists(git_path)){
            std::ifstream head_file(git_path);
            if(head_file.is_open()){
                std::string line;
                if(std::getline(head_file,line)){  
                while(!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')){
                        line.pop_back();
                    }
                    if(line.rfind("ref: refs/heads/",0) == 0){
                        return " ( " + line.substr(16) + " )";
                    }
                    return " (" + line.substr(0,7) + " )";
                }
            }
        }
        if(curr_path == curr_path.root_path() || curr_path.empty())
            break;
        curr_path = curr_path.parent_path();
    }
    return std::string();
}

void handle_sigint(int sig){
    std::cout << "\n" << fetch_username() << fetch_branch() << " > ";
    std::cout.flush();
}

std::vector<std::string> tokenizer(const std::string &command){
    std::vector<std::string> args;
    std::stringstream iss(command);
    std::string tok;
    while(iss >> tok)
        args.push_back(tok);
    return args;
}

void cmd_cd(const std::vector<std::string> &args){
    if(args.size() < 2){
        const char* home = getenv("HOME");
        if(!home) {std::cerr << "home not found \n";}
        std::error_code ec;
        fs::current_path(home,ec);
        if(ec) {std::cerr << "cd: " << ec.message() << "\n";}
        return;
    }
    fs::path target_path = args[1];
    std::error_code ec;
    fs::current_path(target_path,ec);
    if(ec) {std::cerr << "cd: " << ec.message() << "\n";}
}

struct Redirect{
    std::string out_file;
    bool append = false;
    std::string in_file;
};

Redirect parse_redirect(std::vector<std::string> &args){
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
        else if(args[i] == "<" && i + 1 < args.size()){
            r.in_file = args[++i];
        }
        else{
            clean.push_back(args[i]);
        }
    }
    args = clean;
    return r;
}

void apply_redirects(const Redirect r){
    if(!r.out_file.empty()){
        int flags = O_WRONLY | O_CREAT | (r.append ? O_APPEND : O_TRUNC);
        int fd = open(r.out_file.c_str(),flags,0644);
        if(fd < 0) {std::cerr << "failed to open"; _exit(1);}
        dup2(fd, 1);
        close(fd);
    }
    if(!r.in_file.empty()){
        int fd = open(r.in_file.c_str(), O_RDONLY);
        if(fd < 0) {std::cerr << "failed to open"; _exit(1);}
        dup2(fd, 0);
        close(fd);
    }
}

std::vector<std::vector<std::string>> split_pipes(const std::vector<std::string> &args){
    std::vector<std::vector<std::string>> commands;
    std::vector<std::string> current;
    for(const auto &tok: args){
        if(tok == "|"){
            if(!current.empty()){
                commands.push_back(current);
                current.clear();
            }
        }
        else{
            current.push_back(tok);
        }
    }
    if(!current.empty())
    commands.push_back(current);
        return commands;
}

void run_pipeline(std::vector<std::vector<std::string>> &commands){
    int numbers = commands.size();
    int fd[2];
    int prev_read = -1;

    std::vector<pid_t> pids;

    for(int i = 0; i < numbers; i++){
        pipe(fd);

        pid_t pid = fork();

        if(pid == 0){
            if(prev_read != -1){
                dup2(prev_read,0);
                close(prev_read);
            }
            if(i < numbers - 1){
                dup2(fd[1], 1);
            }
            close(fd[1]);
            close(fd[0]);

            Redirect r = parse_redirect(commands[i]);
            apply_redirects(r);

            std::vector<char *> argv;
            for(const auto &a: commands[i]){
                argv.push_back(const_cast<char *>(a.c_str()));
            }
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            std::cerr << "command not found";
            _exit(127);
        }
        else{
            pids.push_back(pid);
            if(prev_read != -1) 
                close(prev_read);
            close(fd[1]);
            prev_read = fd[0];
        }
    }
    if(prev_read != -1) close(prev_read);

    signal(SIGINT, SIG_IGN);
    for(pid_t pid: pids){
        int status;
        waitpid(pid, &status, 0);
    }
    signal(SIGINT, handle_sigint);
}

struct Job{
    pid_t pid;
    std::string command;
    bool stopped;
};

void cmd_jobs(const std::vector<Job> &jobs){
    if(jobs.empty()){
        std::cout << "no jobs \n";
        return;
    }
    for(int i = 0; i < jobs.size(); i++){
        std::cout << "[" << i + 1 << "]"
        << (jobs[i].stopped ? "stopped" : "running")
        << " " << jobs[i].command << "\n";
    }
}

void cmd_fg(std::vector<Job> &jobs, const std::vector<std::string> &args){
    if(jobs.empty()) {std::cerr << "fg: no jobs \n"; return;}

    int idx = jobs.size() - 1;
    if(args.size() >= 2)
        idx = std::stoi(args[1]) - 1;

    if(idx < 0 || idx >= (int)jobs.size()){
        std::cerr << "fg: no such job\n";
        return;
    }

    Job j = jobs[idx];
    jobs.erase(jobs.begin() + idx);

    std::cout << j.command << "\n";
    tcsetpgrp(STDIN_FILENO, j.pid);
    kill(j.pid,SIGCONT);

    signal(SIGINT,SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    int status;
    waitpid(j.pid, &status, WUNTRACED);

    tcsetpgrp(STDIN_FILENO, getpid());

    if(WIFSTOPPED(status)){
        j.stopped = true;
        jobs.push_back(j);
        std::cout << "\n[" << jobs.size() << "] stopped " << j.command << "\n";
    }
    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, SIG_IGN);
}

void cmd_bg(std::vector<Job> &jobs, const std::vector<std::string> &args){
    if(jobs.empty()) {std::cerr << "no jobs found \n"; return;}

    int idx = jobs.size() - 1;
    if(args.size() >= 2)
        idx = std::stoi(args[1]) - 1;

            if(idx < 0 || idx >= (int) jobs.size()){
                std::cerr << "bg: no such job\n";
                return;
            }

            jobs[idx].stopped = false;
            kill(jobs[idx].pid, SIGCONT);
            std::cout << "[" << idx + 1 << "]" << jobs[idx].command << "\n";
}

void run_external(std::vector<std::string> &args, std::vector<Job> &jobs){
    bool background = false;
    if(!args.empty() && args.back() == "&"){
        background = true;
        args.pop_back();
    }
    Redirect r = parse_redirect(args);
    std::vector<char *> argv;
    for(const auto &a: args){
        argv.push_back(const_cast<char *>(a.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();

    if(pid < 0) {std::cerr << "fork failed \n";}

    if(pid == 0){
        setpgid(0, 0);
        signal(SIGINT,SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        apply_redirects(r);
        execvp(argv[0],argv.data());
        std::cerr << args[0] << "command not found";
        _exit(127);
    }

    else{
        if(background){
            Job j;
            j.pid = pid;
            j.command = args[0];
            j.stopped = false;
            jobs.push_back(j);
            std::cout << "[" << jobs.size() << "]" << pid << "\n";
        }
        else{
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, pid);
        signal(SIGINT,SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        int status;
        waitpid(pid, &status, WUNTRACED);

        tcsetpgrp(STDIN_FILENO, getpid());

        if(WIFSTOPPED(status)){
            Job j;
            j.pid = pid;
            j.command = args[0];
            j.stopped = true;
            jobs.push_back(j);
            std::cout << "\n[" << jobs.size() << "] stopped " << j.command << "\n";
        }
        signal(SIGINT,handle_sigint);
        signal(SIGTSTP, SIG_IGN);
    }
    }
}

int main(){
    pid_t shell_pgid = getpid();

    signal(SIGINT,handle_sigint);
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);  

    signal(SIGTSTP, SIG_IGN);    
    signal(SIGTTOU, SIG_IGN);    
    signal(SIGTTIN, SIG_IGN);    

    read_history(".shell_history");

    std::string command;
    std::vector<Job> jobs;
    while(command != "quit" && command != "exit"){
        std::cout << fetch_username() << fetch_branch() << " > ";
        std::getline(std::cin,command);
        if(command.empty())
            continue;
        std::vector<std::string> args = tokenizer(command);
        if(args.empty()){
            continue;
        }
        if(args[0] == "cd"){
            cmd_cd(args);
            continue;
        }
        if(args[0] == "quit" || args[0] == "exit"){
            break;
        }
        if(args[0] == "jobs") {cmd_jobs(jobs); continue;}
        if(args[0] == "fg") {cmd_fg(jobs,args); continue;}
        if(args[0] == "bg") {cmd_bg(jobs, args); continue;}

        std::vector<std::vector<std::string>> commands = split_pipes(args);
        if(commands.size() == 1){
            run_external(args, jobs);
            std::cout << "\n";
        }
        else{
            run_pipeline(commands);
            std::cout << "\n";
        }
    }
    write_history(".shell_history");
    return 0;
}