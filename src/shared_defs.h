// This header is used to share arbitrary definitions between the scheduler and the interpreter,
// but it is also mostly used as an interface for the Process abstract data type.
// Notice that the scheduler and interpreter will not be linked together,
// but both should be linked with process.c
#pragma once

#define EVER ;;  

typedef struct process* Process;

// Creates a new Process.
// A process which does not make reference to another is immutable,
// so after creation its path and policy cannot be changed.
Process create_process(const char* path, unsigned short policy);

// Creates the process with an extra path used for scheduling.
// The MAKES_REFERENCE flag is considered mandatory for the policy argument,
// but for the convenience of the user it is set automatically by the function call
// if it is not set. See Ipath below for details.
// A process with an Ipath can have its policy changed so as to have a new I value, 
// but otherwise it cannot change in any other way.
// See resolve below for details.
Process create_process_with_relative_schedule(const char* path, char* Ipath, unsigned short policy);

// Process policy.
unsigned short policy(Process p);

// In the 16 bits of policy, the least significant 4 bits are for flags.
// In case the REAL_TIME flag is specified, the next 6 bits are for the D option,
// and then the other 6 bits are for the I option.
// Setting 6 bits for each option gives then the range [0-63] as unsigned chars,
// which is more than enough since the restriction is imposed that I + D <= 60.

// If the PRIORITY flag is specified, the next 3 bits after the flags are for priority,
// which for convenience are to be specified using the P0, P1, P2... macros below.
// other bits are meaningless.

// If the ROUND_ROBIN flag is specified, the 12 bits after the flags 
// are used to tell the process scheduler to update the time each process runs in the round robin algorithm (quantum).
// A value of 0 mantains the current time. The time is specified in milliseconds, and ranges in [0-4095].

// Notice the flags ROUND_ROBIN, REAL_TIME and PRIORITY are mutually exclusive,
// and one of them must ALWAYS be specified.

// If the MAKES_REFERENCE flag is specified, the 6 bits after the flags are used for the D option,
// but the remaining bits are meaningless, since we expect the I option to refer to an Ipath instead.
// Although the user is not required to set these bits upon the creation of the process, and they are totally
// unused by the interpreter, the scheduler may nevertheless use these bits to resolve an Ipath into 
// a proper value for I by comparing it against previously obtained executable paths. 
// In that case, the MAKES_REFERENCE flag will be used to indicate that a process is allowed to run earlier
// than the appointed time.

// Example: if a process A is set to run every 1:20 minutes for 5 seconds, and a process B is
// set to run immediately after A (via setting I=A) then the scheduler would resolve I=25 for B.
// However if A blocks or exits before the appointed time (say, in 2 seconds) the scheduler would have a choice
// between waiting the remaining 3 seconds before starting B, or to simply start B.
// Since the creation of the process with I=A sets the MAKES_REFERENCE flag,
// the scheduler will always choose to start B. Otherwise it would wait the 3 seconds.

// Notice however that if the next process after B, say C, does not make reference to B and B ends early due
// to A ending early, C will begin at exactly the same time it would otherwise, since the system would choose
// to wait out for C to start at its appropriate time. This ensures that a process which makes reference to another
// starts immediatly after it ends, but if it specifies an execution time explicitly this is respected by the system.

// If the MAKES_REFERENCE flag is set we know that the process that B refers to is always the 
// last process run before B runs. We assume also that if B makes reference to a process this process
// must be a REAL_TIME process as well, since this is the only way we can resolve I to a proper value and avoid
// conflicts with future REAL_TIME processes.

// MAKES_REFERENCE should only be set if REAL_TIME is set.

//flags
#define REAL_TIME 0x01
#define ROUND_ROBIN 0x02
#define PRIORITY 0x04
#define MAKES_REFERENCE 0x08

// returns true or false depending on whether the flag is set.
#define POLICY_ROUND_ROBIN(x)          ((x) & ROUND_ROBIN)
#define POLICY_REAL_TIME(x)            ((x) & REAL_TIME)
#define POLICY_PRIORITY(x)             ((x) & PRIORITY)
#define POLICY_MAKES_REFERENCE(x)      ((x) & MAKES_REFERENCE)

//obtain values.
#define GET_PRIORITY(x)                (((x) >> 4) & 0x07)
#define GET_D(x)                       (((x) >> 4) & 0x3F)
#define GET_I(x)                       (((x) >> 10) & 0x3F)
#define GET_QUANTUM(x)                 ((x) >> 4)

//set values
#define SET_D(x)                       (((short)((x) & 0x3F)) << 4)
#define SET_I(x)                       (((short)((x) & 0x3F)) << 10)
// notice that unlike the other options which might as well receive unsigned char,
// this option requires an unsigned short.
#define SET_ROBIN_TIME(x)              (((x) & 0x0FFF) << 4)

// priorities (n * 16 == n << 4).
#define P0 0
#define P1 16
#define P2 32
#define P3 48
#define P4 64
#define P5 80
#define P6 96
#define P7 112

// UTILITIES:

// gets start time of a process
#define STIME(p)                             GET_I(policy(p))

// gets duration time of a process.
// total time predicted to be used by process.
#define DTIME(p)                             GET_D(policy(p))

// gets end time of a process
#define ETIME(p)                             (STIME(p) + DTIME(p))

// gets end time of a policy
#define PETIME(x)                             (GET_I(x) + GET_D(x))

// checks whether a Process Makes Reference
#define PMR(p)                                POLICY_MAKES_REFERENCE(policy(p))

// used for switching a process according to its policy.
#define PLP(x)                               ((x) & 0x07)
#define PL(p)                                PLP(policy(p))


// example: 
//     switch (PL(p)){
//         case REAL_TIME: [...]
//         case ROUND_ROBIN: [...]
//         case PRIORITY: [...]
//         case 0: [...] //no flag is set
//         default: [...] //incompatible flags
//     }

// However, the case 0 and default are garanteed never to occur due to the Process implementation, 
// so they need not be treated.

// Usage:
// To create the policy, just use the bitwise OR operator: |
// The order of options is unimportant.

// Examples:
// creates a process with priority 5: P5 | PRIORITY 
// creates a process to run in real time for 10 seconds after every 60 + 20 seconds:
//      REAL_TIME | SET_D(10) | SET_I(20)

// Used to validate a policy.
// Setting incompatible flags, or no flags at all, makes the policy invalid.
// If I + D > 60, the policy is also invalid.
// Notice that if the I option is a path, this does not validate the last restriction.

// Returns an error message string if the policy is invalid
// and NULL if it is valid.
const char* validate_policy(unsigned short policy);

// Calls the error handler with a default message in case a policy is not valid.
// Does nothing otherwise.
void handle_policy(unsigned short policy);

// Path of the process executable.
// Allows relative pathnames and searches executables based on the PATH environment variable.
char* path(Process p);

// Path of the executable that could be specified with the I option.
// This only makes sense when the MAKES_REFERENCE flag is set. 
char* Ipath(Process p);

// An Ipath process, which makes reference to another, can be changed by this routine
// so as to have its STIME value changed to start_time.
// An error will be generated if the MAKES_REFERENCE flag is off.
void resolve(Process p, unsigned char start_time);

// Free the memory associated with the process.
void free_process(Process p);

// Deep copy of processes.
Process process_deep_copy(Process p);

// Print process to stdout.
void print_process(Process p);

// exception handler.
void handle(char* message, ...);