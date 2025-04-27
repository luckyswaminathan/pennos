# PennOS Companion Document

**Note:** This document should be converted to PDF format for submission.

## OS API and Functionality

*(This section should provide a detailed description of the Application Programming Interface (API) exposed by your PennOS implementation and explain the functionality of the core components.)*

### Shell (`src/shell/`)

*   Describe the commands supported by the shell.
*   Explain how the shell parses commands and interacts with other OS components (e.g., file system, process management).
*   Detail any special features (e.g., I/O redirection, background processes).

### File System (PennFAT - `src/pennfat/`)

*   Describe the structure of your PennFAT implementation (e.g., Superblock, FAT, Directory Entries, Data Blocks).
*   List and explain the file system system calls/API functions (e.g., `p_open`, `p_read`, `p_write`, `p_close`, `p_mkdir`, `p_rmdir`, `p_ls`, `p_rm`, `p_fstat`, etc.).
*   Explain how these functions interact with the on-disk structures.
*   Discuss any limitations or design choices.

### Process Management & Scheduling (`src/scheduler/`)

*   Describe the process control block (PCB) structure.
*   Explain the scheduling algorithm implemented (e.g., Round Robin, Priority-based).
*   List and explain the process management system calls/API functions (e.g., `p_spawn`, `p_wait`, `p_kill`, `p_nice`, `p_sleep`, `p_ps`, etc.).
*   Detail how context switching is handled.
*   Explain inter-process communication (IPC) mechanisms, if any.

### Utility Functions (`src/utils/`)

*   Describe any significant utility functions or modules that support the core components.

*(Ensure this document thoroughly covers the API and functionality as required by the assignment specifications.)* 