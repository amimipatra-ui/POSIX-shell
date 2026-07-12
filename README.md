# ntime shell

# Introduction

a POSIX shell implementation written in c++17;

## Features

- cmd exec via `fork()` / `execvp()`
- pipes & I/O redirections (`ls | grep .cpp | wc - l`) (`>`, `>>`, `<`)
- signal handling (`Ctrl + C` survives)
- job control (`sleep 200 &`)
- `cd` builtin with home direc support
- git branch display in prompt

## Build

```bash
clang++ -std=c++17 shell.cpp -o shell
./shell
```
