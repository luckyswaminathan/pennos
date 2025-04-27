# PennOS Project Readme

## Name and Penn Key

*   **Name:** [Your Name Here]
*   **Penn Key:** [Your Penn Key Here]

## Submitted Source Files

*   `Makefile`
*   `scheduler.mk`
*   `src/Makefile`
*   `src/pennosfat`
*   `src/shell/...` (all files within)
*   `src/utils/...` (all files within)
*   `src/scheduler/...` (all files within)
*   `src/pennfat/...` (all files within)
*   `lib/...` (relevant library files if any)
*   `tests/...` (all files within)
*   `doc/companion.md` (or `doc/companion.pdf` after conversion)

*(Please list any other relevant files you submitted)*

## Extra Credit Answers

*(Detail any extra credit implemented here)*

## Compilation Instructions

1.  Navigate to the `src` directory: `cd src`
2.  Run `make` to compile the project: `make`
3.  The executable(s) (e.g., `pennosfat`) will be created in the `src` directory.

*(Add any specific targets or variations if needed, e.g., `make clean`, `make test`)*

## Overview of Work Accomplished

*(Provide a high-level summary of the features and components implemented in this submission. e.g., implemented the FAT file system, the shell, process scheduling, etc.)*

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