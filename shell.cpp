#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <vector>
#include <fcntl.h>

namespace fs = std::filesystem;

std::string fetch_path(){
    fs::path current_file = fs::current_path();
    fs::path chopped_path;
    int count = 0;
    for(const auto &path: current_file){
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
                        return " (" + line.substr(16) + ") ";
                    }
                    return "(" + line.substr(0,7) + ")";
                }
            }
        }
        if(current_path == current_path.root_path() || current_path.empty()){
            break;
        }
    current_path = current_path.parent_path();
    } 
    return std::string();
}

std::vector<std::string> tokenizer(const std::string &command){
    std::vector<std::string> args;
    std::stringstream iss(command);
    std::string tok;
    while(iss >> tok)
        args.push_back(tok);
    return args;
}

void cmd_cd(const std::vector<std::string> args){
    if(args.size() < 2){
        const char *home = getenv("HOME");
        if(!home){std::cerr << "home not found";}
        std::error_code ec;
        fs::current_path(home,ec);
        if(ec) {std::cerr << "cd: " << ec.message() << "not found";}
        return;
    }

    fs::path target = args[1];
    std::error_code ec;
    fs::current_path(target,ec);
    if(ec) {std::cerr << "cd: " << ec.message() << "not found";}
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
        if(args[i] == ">" && i < args.size() - 1){
            r.out_file = args[++i];
        }
        else if(args[i] == ">>" && i < args.size()){
            r.out_file = args[++i];
            r.append = true;
        }
        else if (args[i] == "<" && i < args.size()) {
            r.in_file = args[++i];
        }
        else{
            clean.push_back(args[i]);
        }
    }
    args = clean;
    return r;
}

void apply_redirects(Redirect r){
    if(!r.out_file.empty()){
        int flags = O_WRONLY | O_CREAT | (r.append ? O_APPEND : O_TRUNC);
        int fd = open(r.out_file.c_str(), flags, 0677);
        if(fd < 0) {std::cerr << "no command found";}
        dup2(fd, 1);
        close(fd);
    }
    if(!r.in_file.empty()){
        int fd = open(r.in_file.c_str(), O_RDONLY);
        if(fd < 0) {
        std::cerr << "cannot open " << r.in_file << ": " << strerror(errno) << "\n";
        _exit(1);
        }
        dup2(fd, 0);
        close(fd);
    }
}

std::vector<std::vector<std::string>> split_pipes(const std::vector<std::string> &args){
    std::vector<std::vector<std::string>> commands;
    std::vector<std::string> current;
    for(const auto &tok: args){
            if(tok == "|"){
                commands.push_back(current);
                current.clear();
            }else{
                current.push_back(tok);
            }
        }
    if(!current.empty())
        commands.push_back(current);
    return commands;
}

void run_pipeline(std::vector<std::vector<std::string>> commands ){
    int size = commands.size();
    int fd[2];
    int prev_read = -1;

    std::vector<pid_t> pids;

    for(int i = 0; i < size; i++){
        pipe(fd);

        pid_t pid = fork();

        if(pid < 0) {std::cerr << "fork faled";}

        if(pid == 0){
            if(prev_read != -1){
                dup2(prev_read, 0);
                close(prev_read);;
            }
            if(i < size - 1){
                dup2(fd[1], 1);
            }
            close(fd[0]);
            close(fd[1]);

            Redirect r = parse_redirect(commands[i]);
            apply_redirects(r);

            std::vector<char *> argv;
            for(const auto &a: commands[i]){
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            std::cerr << "command not found";
            _exit(127);
        }
        else{
            pids.push_back(pid);
            if(prev_read != -1) close(prev_read);
            close(fd[1]);
            prev_read = fd[0];
        }
    }
        for(pid_t pid : pids){
        int status;
        waitpid(pid, &status, 0);
    }
}

void cmd_runExternal(std::vector<std::string> &args){
    Redirect r = parse_redirect(args);
    std::vector<char*> argv;
    for(const auto &a: args){
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();

    if(pid < 0) {std::cerr << "fork failed \n"; }

    if(pid == 0){
        apply_redirects(r);
        execvp(argv[0], argv.data());
        std::cerr << "command not found\n";
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
}


int main(){
    std::string command;
    while(command != "quit" && command != "exit"){ 
        std::cout << fetch_path() << fetch_branch() <<  " > ";
        std::getline(std::cin,command);
        if(command.empty())
            continue;
        std::vector<std::string> args = tokenizer(command);
        if(args[0].empty()) continue;  //check the whitespaces later
        if(args[0] == "quit" || args[0] == "exit") break;
        if(args[0] == "cd"){ cmd_cd(args); continue;}
        std::vector<std::vector<std::string>> commands = split_pipes(args);
        if(commands.size() == 1){
            cmd_runExternal(args);
            std::cout << "\n";
        }
        else{
            run_pipeline(commands);
            std::cout << "\n";
        }
    }
}