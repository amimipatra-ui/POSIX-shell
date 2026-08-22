# ntime shell

# Introduction

a POSIX shell implementation written in c++17;

## Features

v0.0.0:

- cmd exec via `fork()` / `execvp()`
- pipes & I/O redirections (`ls | grep .cpp | wc - l`) (`>`, `>>`, `<`)
- git branch display in prompt

## Build

```bash
clang++ -std=c++17 shell.cpp -o shell
./shell
```
