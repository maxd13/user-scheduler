#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "shared_defs.h"

#define MAX_PATH 100 // maximum size of a file path

struct process{
    // path to the executable process. 
    // Allows relative pathnames and searchs executables based on the PATH envinroment variable.
    char path[MAX_PATH];
    char Ipath[MAX_PATH]; // path that could be specified with the I option.
    unsigned short policy; // this is configured by bits. See the macros in shared_defs.h for details.
};

// Creates a new Process.
// A process is immutable, so after creation its path and policy cannot be changed.
Process create_process(const char* path, unsigned short policy){
    Process new;
    if(strlen(path) > MAX_PATH) handle("path is too big for buffer: %s\n", path);
    handle_policy(policy);
    new = (Process) malloc(sizeof(struct process));
    if(!new) handle("no memory to create process at %s\n", path);
    strcpy(new->path, path);
    strcpy(new->Ipath, "");
    new->policy = policy;
    return new;
}

// Creates the process with an extra path used for scheduling.
// See Ipath below for details.
Process create_process_with_relative_schedule(const char* path, char* Ipath, unsigned short policy){
    Process new;
    if(strlen(Ipath) > MAX_PATH) handle("Ipath is too big for buffer: %s\n", Ipath);
    policy = policy | MAKES_REFERENCE;
    new = create_process(path, policy);
    strcpy(new->Ipath, Ipath);
    return new;
}

// Process policy.
unsigned short policy(Process p){
    return p->policy;
}

// used to validate a policy.
// returns an error message if the policy is invalid
// and an empty string if it is valid.
// Setting incompatible flags makes the policy invalid.
const char* validate_policy(unsigned short policy){
    if(POLICY_MAKES_REFERENCE(policy) && !POLICY_REAL_TIME(policy))
        return "MAKES_REFERENCE flag specified, but policy is not REAL_TIME.";
    switch (policy & 0x07){
        case REAL_TIME:
            if (POLICY_MAKES_REFERENCE(policy)){
                if(GET_D(policy) > 60)
                    return "policy would take more than one minute to run.";
            }
            else if(GET_D(policy) + GET_I(policy) > 60)
                return "policy would take more than one minute to run.";
            break;
        case ROUND_ROBIN:
            break;
        case PRIORITY:
            if(GET_PRIORITY(policy) > 7)
                return "unknown priority level.";
            break;
        case 0:
            return "no flag is set.";
        default:
            return "incompatible flags.";
    }
    return NULL;
}

// Calls the error handler with a default message in case a policy is not valid.
// Does nothing otherwise.
void handle_policy(unsigned short policy){
    char* msg;
    if(msg = validate_policy(policy)) 
        handle("policy is not valid: %s\n", msg);
}

// Path of the process executable.
// Allows relative pathnames and searches executables based on the PATH environment variable.
char* path(Process p){
    return p->path;
}

// Path of the executable that could be specified with the I option.
// This only makes sense when the MAKES_REFERENCE flag is set. 
char* Ipath(Process p){
    return p->Ipath;
}

// Free the memory associated with the process.
void free_process(Process p){
    free(p);
}

// exception handler.
void handle(char* message, ...){
	va_list argptr;
	va_start(argptr, message);
	vfprintf(stderr, message, argptr);
	va_end(argptr);
	exit(EXIT_FAILURE);
}