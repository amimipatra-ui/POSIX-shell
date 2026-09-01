# POSIX shell

A POSIX-style shell written from scratch in C++17 — process management,
job control, and line editing implemented directly on top of Unix syscalls,
no shell-building libraries used.

## Why

Built as a systems-programming project to understand what a shell actually
does under the hood: process creation, signal delivery, terminal ownership,
and job control — the mechanisms every Unix shell relies on but few people
ever implement themselves.

## Features

- Command execution via `fork()` / `execvp()`
- Pipelines (`ls | grep .cpp | wc -l`)
- I/O redirection (`>`, `>>`, `<`)
- Job control: background execution (`&`), `jobs`, `fg`, `bg`, Ctrl+Z suspend/resume
- Correct signal handling — Ctrl+C kills the foreground job without killing the shell
- Process groups + terminal ownership (`setpgid`, `tcsetpgrp`) for job control
- Readline integration: command history, line editing, tab completion (builtins + `$PATH`)
- Git-aware prompt (shows current branch)
- Quotting escaping
- Variable expansion , exit status tracking matching real shell conventions (`128 + signal`)
- Glob expamsion via `glob()` API
- Alias/Unalias support

## Demo

![demo](sample.gif)

## Build

Requires GNU readline (not the macOS-default libedit).

```bash
brew install readline   # macOS only, if not already installed

clang++ -std=c++17 \
  -I/opt/homebrew/opt/readline/include \
  -L/opt/homebrew/opt/readline/lib \
  -lreadline \
  shell.cpp -o shell

./shell
```

Or, using the included Makefile:

```bash
make run
```

## Usage

```bash
sleep 100 &      # run in background
jobs             # list background/stopped jobs
fg               # bring most recent job to foreground
bg               # resume a stopped job in the background
```

## Roadmap

- [ ] Pipeline + job control integration (background/stop a piped command)
- [ ] Filename completion (currently completes commands only)
- [ ] Aliases / environment variable expansion
- [ ] Config file (`.ntimerc`)
