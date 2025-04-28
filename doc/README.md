# PennOS Project Readme

## Name and Penn Key

*   **Names:**: Aagam Dalal, Lakshman Swaminathan, Kavin Sarvanan, Arnav Chopra
*   **Penn Key:** [Your Penn Key Here]

## Directory Structure and Files Submitted

```
├── Makefile
├── Makefile2
├── README.md
├── bin
│   ├── pennfat
│   └── standalone-pennfat
├── doc
│   ├── README.md
│   └── companion.md
├── lib
│   ├── exiting_alloc.o
│   └── linked_list.h
├── llvm.sh.1
├── pennosfat
├── scheduler-test
├── scheduler.mk
├── src
│   ├── Makefile
│   ├── nonewline
│   ├── nonewline2
│   ├── pennfat
│   │   ├── fat.c
│   │   ├── fat.h
│   │   ├── fat.o
│   │   ├── fat_constants.h
│   │   ├── fat_utils.c
│   │   ├── fat_utils.h
│   │   ├── fat_utils.o
│   │   ├── mkfs.c
│   │   ├── mkfs.h
│   │   ├── mkfs.o
│   │   └── pennfat.c
│   ├── pennosfat
│   ├── scheduler
│   │   ├── README.md
│   │   ├── fat_syscalls.c
│   │   ├── fat_syscalls.h
│   │   ├── fat_syscalls.o
│   │   ├── kernel.c
│   │   ├── kernel.h
│   │   ├── kernel.o
│   │   ├── logger.c
│   │   ├── logger.h
│   │   ├── logger.o
│   │   ├── pennos.c
│   │   ├── sched-test.o
│   │   ├── scheduler.c
│   │   ├── scheduler.h
│   │   ├── scheduler.o
│   │   ├── spthread.c
│   │   ├── spthread.h
│   │   ├── spthread.o
│   │   ├── spthread_demo.c
│   │   ├── sys.c
│   │   ├── sys.h
│   │   └── sys.o
│   ├── scheduler.log
│   ├── shell
│   │   ├── Job.c
│   │   ├── Job.h
│   │   ├── Job.o
│   │   ├── Makefile
│   │   ├── README.md
│   │   ├── command_execution.c
│   │   ├── command_execution.h
│   │   ├── command_execution.o
│   │   ├── commands.c
│   │   ├── commands.h
│   │   ├── commands.o
│   │   ├── exiting_signal.c
│   │   ├── exiting_signal.h
│   │   ├── exiting_signal.o
│   │   ├── inp.txt
│   │   ├── jobs.c
│   │   ├── jobs.h
│   │   ├── jobs.o
│   │   ├── out.txt
│   │   ├── parser.c
│   │   ├── parser.h
│   │   ├── parser.o
│   │   ├── penn-shell
│   │   ├── penn-shell.c
│   │   ├── penn-shell.o
│   │   ├── print.o
│   │   ├── shell_porcelain.c
│   │   ├── shell_porcelain.h
│   │   ├── shell_porcelain.o
│   │   ├── signals.c
│   │   ├── signals.h
│   │   ├── signals.o
│   │   ├── stress.c
│   │   ├── stress.h
│   │   ├── stress.o
│   │   ├── valid_input.c
│   │   ├── valid_input.h
│   │   └── valid_input.o
│   └── utils
│       ├── errno.c
│       ├── errno.h
│       ├── errno.o
│       ├── error_codes.h
│       ├── spthread.c
│       └── spthread.h
├── test
│   └── sched-demo.c
├── test_scheduler
└── tests
    ├── pennfat
    │   ├── acutest.h
    │   ├── cram-tests
    │   │   └── mount_unmount.t
    │   ├── test_pennfat
    │   └── test_pennfat.c
    ├── sched-demo.c
    ├── test_linked_list.c
    ├── test_scheduler.c
    └── test_scheduler.o
```

## Compilation Instructions

1.  Navigate to the `src` directory: `cd src`
2.  Run `make` to compile the project: `make`
3.  The executable(s) (e.g., `pennosfat`) will be created in the `src` directory.

## Overview of Work Accomplished

This project implements a basic operating system with a filesystem that lives on top (and runs as a process in) the host operating system. Users can interact with it via a shell interface that is also implemented. Here is a breakdown of the different components we have implemented -

### Process Management

We have implemented a priority based scheduler for our operating system that keeps maintains queues for each of these states -
- High Priority
- Medium Priority
- Low Priority
- Blocked
- Stopped
- Zombied

Within each queue, we use round robin scheduling to schedule processes. Two processes are always running in the lifetime of the OS (while the user is interacting with the shell) - the INIT process and the SHELL process. These are both scheduled with the highest priority.
- INIT is responsible for reaping zombies and for spawning the shell process. It is only scheduled when there are zombies to reap, as to ensure that it does not busy wait and consume CPU time.
- SHELL is responsible for parsing commands and executing them. It also spawns new processes when necessary.

We maintain three levels of abstraction for our processes -
- Kernel Level Functions (`k_`): These functions are used to interface directly with the scheduler
- System Calls (`s_`): These functions implement higher level functionality built on top of the kernel level functions
- User Level Functions: These functions are used to interface with the shell and use the system calls to interact with the kernel

### Shell Interface

### FAT based Filesystem 

aagam here plz


## Description of Code and Code Layout

The project is structured as follows:

*   **`/` (Root Directory):** Contains the main Makefile, test infrastructure, and documentation.
*   **`src/`:** Contains the core source code for PennOS.
    *   `Makefile`: Specific makefile for compiling the source within `src`.
    *   `shell/`: Source code for the PennOS shell implementation.
    *   `utils/`: Utility functions used across different components.
    *   `scheduler/`: Source code for the process scheduler.
    *   `pennfat/`: Source code for the PennFAT file system implementation.
    *   `pennosfat`: Likely the main executable or related file.
*   **`lib/`:** Contains any supporting libraries.
*   **`tests/` (and potentially `test/`):** Contains testing scripts and code.
*   **`doc/`:** Contains documentation, including the Companion Document.
*   **`bin/`:** May contain compiled binaries or scripts.

*(Add more specific details about key modules or design choices)*

## General Comments

*(Include any other relevant information, known issues, or comments that might help the graders understand your submission.)*

## Companion Document

A detailed description of the OS API and functionality can be found in the Companion Document located at `doc/companion.md`.