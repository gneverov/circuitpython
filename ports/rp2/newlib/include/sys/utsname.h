// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#define UTSNAME_LENGTH 16


struct utsname {
    char sysname[UTSNAME_LENGTH];  // Name of this implementation of the operating system.
    char nodename[UTSNAME_LENGTH];  // Name of this node within the communications network to which this node is attached, if any.
    char release[UTSNAME_LENGTH];  // Current release level of this implementation.
    char version[UTSNAME_LENGTH];  // Current version level of this release.
    char machine[UTSNAME_LENGTH];  // Name of the hardware type on which the system is running.
};

int uname(struct utsname *name);
