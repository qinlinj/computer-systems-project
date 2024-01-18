/**
 * @file tsh.c
 * @brief A tiny shell program with job control
 *
 * This program implements a simple Unix shell with job control features.
 * It allows users to run commands in the foreground and background, manage
 * multiple jobs, and perform basic input/output redirection. The shell supports
 * built-in commands like "jobs," "bg," and "fg" for controlling jobs.
 *
 * The shell provides a command-line interface where users can enter commands
 * to be executed. It parses user input, handles job execution, and manages
 * the execution of foreground and background jobs. It also handles signals
 * like SIGINT (Ctrl+C), SIGTSTP (Ctrl+Z), and SIGCHLD for job control.
 *
 * This shell program is designed to be a minimalistic example of a Unix shell
 * and can serve as a learning resource for understanding the basics of shell
 * implementation and process management.
 *
 * @author Qinlin Jia <qinlinj@andrew.cmu.edu>
 */

#include "csapp.h"
#include "tsh_helper.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void cleanup(void);

void init_signal_sets(sigset_t *mask_all, sigset_t *mask_three);
void handle_builtin_jobs(const struct cmdline_tokens token,
                         sigset_t *mask_three);
void handle_builtin_bg_fg(const struct cmdline_tokens token, sigset_t *mask_all,
                          sigset_t *mask_three, parseline_return parse_result);
void handle_builtin_none(const struct cmdline_tokens token,
                         parseline_return parse_result, const char *cmdline,
                         sigset_t *mask_all, sigset_t *mask_three);
void redirect_io(const char *filename, int filedes);
pid_t parse_argument(const struct cmdline_tokens tokens,
                     sigset_t *previousMask);
void handle_job(pid_t processId, const struct cmdline_tokens tokens,
                sigset_t *previousMask);
void print_job(jid_t jobId, pid_t processId);
void execute_command(const struct cmdline_tokens tokens,
                     sigset_t *previousMask);
void handle_parent_process(pid_t processId, parseline_return parseResult,
                           const char *cmdLine, sigset_t *previousMask);

/**
 * @brief Main function of the shell.
 *
 * This function serves as the entry point for the shell program. It handles
 * command line argument parsing, sets up signal handlers, initializes the job
 * list, and enters the shell's read/eval loop to process user commands.
 *
 * @param argc The number of command line arguments.
 * @param argv An array of strings representing the command line arguments.
 * @return Returns 0 upon successful execution.
 */
int main(int argc, char **argv) {
    int c;
    char cmdline[MAXLINE_TSH]; // Cmdline for fgets
    bool emit_prompt = true;   // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0) {
        perror("dup2 error");
        exit(1);
    }

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h': // Prints help message
            usage();
            break;
        case 'v': // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p': // Disables prompt printing
            emit_prompt = false;
            break;
        default:
            usage();
        }
    }

    // Create environment variable
    if (putenv(strdup("MY_ENV=42")) < 0) {
        perror("putenv error");
        exit(1);
    }

    // Set buffering mode of stdout to line buffering.
    // This prevents lines from being printed in the wrong order.
    if (setvbuf(stdout, NULL, _IOLBF, 0) < 0) {
        perror("setvbuf error");
        exit(1);
    }

    // Initialize the job list
    init_job_list();

    // Register a function to clean up the job list on program termination.
    // The function may not run in the case of abnormal termination (e.g. when
    // using exit or terminating due to a signal handler), so in those cases,
    // we trust that the OS will clean up any remaining resources.
    if (atexit(cleanup) < 0) {
        perror("atexit error");
        exit(1);
    }

    // Install the signal handlers
    Signal(SIGINT, sigint_handler);   // Handles Ctrl-C
    Signal(SIGTSTP, sigtstp_handler); // Handles Ctrl-Z
    Signal(SIGCHLD, sigchld_handler); // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler);

    // Execute the shell's read/eval loop
    while (true) {
        if (emit_prompt) {
            printf("%s", prompt);

            // We must flush stdout since we are not printing a full line.
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
            exit(1);
        }

        if (feof(stdin)) {
            // End of file (Ctrl-D)
            printf("\n");
            return 0;
        }

        // Remove any trailing newline
        char *newline = strchr(cmdline, '\n');
        if (newline != NULL) {
            *newline = '\0';
        }

        // Evaluate the command line
        eval(cmdline);
    }

    return -1; // control never reaches here
}

/**
 * @brief Evaluates and handles user commands.
 *
 * This function parses the user-provided command line, identifies built-in
 * commands, and executes external commands as appropriate. It also handles job
 * control operations and signals. Detects and handles errors.
 *
 * @param cmdLine The command line string provided by the user.
 */
void eval(const char *cmdLine) {
    parseline_return parseResult;
    struct cmdline_tokens tokens;
    sigset_t maskAll, maskSelected;

    // Initialization of signal sets
    init_signal_sets(&maskAll, &maskSelected);

    // Parse command line
    parseResult = parseline(cmdLine, &tokens);

    if (parseResult == PARSELINE_ERROR || parseResult == PARSELINE_EMPTY) {
        return;
    }

    // Handling built-in commands
    switch (tokens.builtin) {
    case BUILTIN_QUIT:
        exit(0);
        break;
    case BUILTIN_JOBS:
        handle_builtin_jobs(tokens, &maskSelected);
        break;
    case BUILTIN_BG:
    case BUILTIN_FG:
        handle_builtin_bg_fg(tokens, &maskAll, &maskSelected, parseResult);
        break;
    case BUILTIN_NONE:
        handle_builtin_none(tokens, parseResult, cmdLine, &maskAll,
                            &maskSelected);
        break;
    }

    return;
}

/*****************
 * Signal handlers
 *****************/

/**
 * @brief Handles the SIGCHLD signal.
 *
 * This function is called when a child process stops, terminates due to a
 * signal, or exits normally. It reaps child processes using waitpid, updates
 * job states, and deletes jobs from the job list as appropriate. It also logs
 * which signal caused the child process to change state.
 *
 * @param sig The signal number that caused the handler to be invoked.
 */
void sigchld_handler(int sig) {
    int savedErrno = errno; // Preserve original errno value
    int childStatus;
    pid_t childPid;
    jid_t jobID;

    // Block all signals
    sigset_t allSignalsMask, previousSignalMask;
    sigfillset(&allSignalsMask);
    sigprocmask(SIG_BLOCK, &allSignalsMask,
                &previousSignalMask); // Block all signals

    while ((childPid = waitpid(-1, &childStatus, WNOHANG | WUNTRACED)) > 0) {
        jobID = job_from_pid(childPid);

        if (WIFSTOPPED(childStatus)) {
            // Child process was stopped
            job_set_state(jobID, ST);
            sio_printf("Job [%d] (%d) stopped by signal %d\n", jobID, childPid,
                       WSTOPSIG(childStatus));
        } else if (WIFSIGNALED(childStatus)) {
            // Child process was terminated by a signal
            sio_printf("Job [%d] (%d) terminated by signal %d\n", jobID,
                       childPid, WTERMSIG(childStatus));
            delete_job(jobID);
        } else if (WIFEXITED(childStatus)) {
            // Child process exited normally
            delete_job(jobID);
        }
    }

    sigprocmask(SIG_SETMASK, &previousSignalMask,
                NULL);  // Restore previous signal mask
    errno = savedErrno; // Restore original errno
}

/**
 * @brief Handles the SIGINT signal.
 *
 * This signal handler is invoked when the user sends an interrupt signal
 * (SIGINT). It forwards the received signal to the foreground job, if there is
 * one. The function blocks all signals during this operation to avoid race
 * conditions.
 *
 * @param sig The received signal number (should be SIGINT).
 */
void sigint_handler(int sig) {
    int savedErrno = errno; // Preserve original errno value
    sigset_t allSignalsMask, previousMask;
    pid_t foregroundPid;
    jid_t foregroundJobId;

    sigfillset(&allSignalsMask);
    sigprocmask(SIG_BLOCK, &allSignalsMask, &previousMask); // Block all signals

    foregroundJobId = fg_job(); // Get the foreground job ID
    if (foregroundJobId > 0) {
        foregroundPid =
            job_get_pid(foregroundJobId); // Get the PID of the foreground job
        kill(-foregroundPid, sig); // Forward the received signal to the entire
                                   // foreground process group
    }

    sigprocmask(SIG_SETMASK, &previousMask,
                NULL);  // Restore previous signal mask
    errno = savedErrno; // Restore original errno
}

/**
 * @brief Handles the SIGTSTP signal.
 *
 * This signal handler is invoked when the user sends a stop signal (SIGTSTP).
 * It sends the SIGTSTP signal to the foreground job, if there is one, to stop
 * it. The function blocks all signals during this operation to avoid race
 * conditions.
 *
 * @param sig The signal number (should be SIGTSTP).
 */
void sigtstp_handler(int sig) {
    int savedErrno = errno; // Preserve original errno value
    sigset_t allSignalsMask, previousMask;
    pid_t foregroundPid;
    jid_t foregroundJobId;

    sigfillset(&allSignalsMask);
    sigprocmask(SIG_BLOCK, &allSignalsMask, &previousMask); // Block all signals

    foregroundJobId = fg_job(); // Get the foreground job ID
    if (foregroundJobId > 0) {
        foregroundPid =
            job_get_pid(foregroundJobId); // Get the PID of the foreground job
        kill(-foregroundPid,
             SIGTSTP); // Send SIGTSTP to the entire foreground process group
    }

    sigprocmask(SIG_SETMASK, &previousMask,
                NULL);  // Restore previous signal mask
    errno = savedErrno; // Restore original errno
}

/**
 * @brief Attempt to clean up global resources when the program exits.
 *
 * In particular, the job list must be freed at this time, since it may
 * contain leftover buffers from existing or even deleted jobs.
 */
void cleanup(void) {
    // Signals handlers need to be removed before destroying the joblist
    Signal(SIGINT, SIG_DFL);  // Handles Ctrl-C
    Signal(SIGTSTP, SIG_DFL); // Handles Ctrl-Z
    Signal(SIGCHLD, SIG_DFL); // Handles terminated or stopped child

    destroy_job_list();
}

/*****************
 * Helper functions
 *****************/

/**
 * Initializes two sets of signals: one for blocking all signals and another for
 * blocking specific signals.
 *
 * @param maskAll A pointer to the signal set that will be initialized to block
 * all signals.
 * @param maskSelected A pointer to the signal set that will be initialized to
 * block specific signals (SIGINT, SIGCHLD, SIGTSTP).
 */
void init_signal_sets(sigset_t *maskAll, sigset_t *maskSelected) {
    sigfillset(maskAll); // Initialize the maskAll set to block all signals
    sigemptyset(maskSelected); // Initialize maskSelected to an empty set

    // Add specific signals to the maskSelected set
    sigaddset(maskSelected, SIGINT);  // Add SIGINT to the set
    sigaddset(maskSelected, SIGCHLD); // Add SIGCHLD to the set
    sigaddset(maskSelected, SIGTSTP); // Add SIGTSTP to the set
}

/**
 * Handles the built-in 'jobs' command of the shell.
 *
 * Lists all the jobs currently managed by the shell. If an output file is
 * specified, the job list is redirected to the file.
 *
 * @param tokens Struct containing parsed command line tokens.
 * @param maskSelected A pointer to the signal set used for blocking specific
 * signals during execution.
 */
void handle_builtin_jobs(const struct cmdline_tokens tokens,
                         sigset_t *maskSelected) {
    int fileDescriptor;
    sigset_t previousMask;

    // Block specific signals during the execution of this command
    sigprocmask(SIG_BLOCK, maskSelected, &previousMask);

    if (tokens.outfile == NULL) {
        // If no output file specified, list jobs to standard output
        list_jobs(STDOUT_FILENO);
    } else {
        // Open the specified output file with appropriate permissions
        fileDescriptor =
            open(tokens.outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fileDescriptor < 0) {
            // Handle error in opening file
            perror(tokens.outfile);
            return;
        }
        // Redirect job listing to the file
        list_jobs(fileDescriptor);
        close(fileDescriptor); // Close the file after writing
    }

    // Restore the previous signal mask
    sigprocmask(SIG_SETMASK, &previousMask, NULL);
}

/**
 * Handles the built-in 'bg' and 'fg' commands of the shell.
 *
 * Moves jobs to the foreground or background, and continues their execution if
 * stopped.
 *
 * @param tokens Struct containing parsed command line tokens.
 * @param maskAll A pointer to the signal set used for blocking all signals
 * during execution.
 * @param maskSelected A pointer to the signal set used for blocking specific
 * signals during execution.
 * @param parseResult The result of the command line parsing, indicating whether
 * the command is to be run in foreground or background.
 */
void handle_builtin_bg_fg(const struct cmdline_tokens tokens, sigset_t *maskAll,
                          sigset_t *maskSelected,
                          parseline_return parseResult) {
    sigset_t previousMask;
    pid_t processId;

    // Check if there are enough arguments
    if (tokens.argc < 2) {
        printf("%s command requires PID or %%jobid argument\n", tokens.argv[0]);
        return;
    }

    // Block signals
    sigprocmask(SIG_BLOCK, maskAll, &previousMask);

    // Parse the argument and get the process ID
    processId = parse_argument(tokens, &previousMask);
    if (processId == -1) {
        return;
    }

    // Handle the job based on the command (BG or FG)
    handle_job(processId, tokens, &previousMask);

    // Restore previous signal mask
    sigprocmask(SIG_SETMASK, &previousMask, NULL);
}

/**
 * Parses the command line argument to obtain the process ID or job ID.
 *
 * @param tokens Struct containing parsed command line tokens.
 * @param previousMask A pointer to the previous signal mask, used for signal
 * handling.
 * @return The process ID or -1 in case of an error.
 */
pid_t parse_argument(const struct cmdline_tokens tokens,
                     sigset_t *previousMask) {
    char *arg = tokens.argv[1];
    pid_t processId;
    jid_t jobId;

    // Check if the argument starts with '%', indicating a job ID
    if (arg[0] == '%') {
        jobId = atoi(arg + 1);
        if (jobId == 0 || !job_exists(jobId)) {
            printf("%s: No such job\n", arg);
            sigprocmask(SIG_SETMASK, previousMask, NULL);
            return -1;
        }
        processId = job_get_pid(jobId);
    } else {
        // Parse the argument as a PID
        processId = atoi(arg);
        if (processId == 0) {
            printf("%s: argument must be a PID or %%jobid\n", tokens.argv[0]);
            sigprocmask(SIG_SETMASK, previousMask, NULL);
            return -1;
        }
        jobId = job_from_pid(processId);
        if (jobId == 0) {
            printf("%s: No such process\n", arg);
            sigprocmask(SIG_SETMASK, previousMask, NULL);
            return -1;
        }
    }

    return processId;
}

/**
 * Handles a job (process) based on whether it's a foreground or background job.
 *
 * @param processId The process ID of the job to be handled.
 * @param tokens Struct containing parsed command line tokens.
 * @param previousMask A pointer to the previous signal mask, used for signal
 * handling.
 */
void handle_job(pid_t processId, const struct cmdline_tokens tokens,
                sigset_t *previousMask) {
    jid_t jobId = job_from_pid(processId);
    enum job_state newState = (tokens.builtin == BUILTIN_BG) ? BG : FG;
    job_set_state(jobId, newState);
    kill(-processId, SIGCONT);

    if (newState == FG) {
        // If it's a foreground job, wait for it to complete
        while (fg_job()) {
            sigsuspend(previousMask);
        }
    } else {
        // If it's a background job, print job information
        print_job(jobId, processId);
    }
}

/**
 * Prints job information, including job ID, process ID, and command line.
 *
 * @param jobId The job ID.
 * @param processId The process ID.
 */
void print_job(jid_t jobId, pid_t processId) {
    printf("[%d] (%d) %s\n", jobId, processId, job_get_cmdline(jobId));
}

/**
 * Handles commands that are not built-in to the shell.
 *
 * Executes external commands by forking a child process. Handles I/O
 * redirection if specified.
 *
 * @param tokens Struct containing parsed command line tokens.
 * @param parseResult The result of the command line parsing, indicating whether
 * the command is to be run in foreground or background.
 * @param cmdLine The original command line string.
 * @param maskAll A pointer to the signal set used for blocking all signals
 * during the child process creation.
 * @param maskSelected A pointer to the signal set used for blocking specific
 * signals during execution.
 */
void handle_builtin_none(const struct cmdline_tokens tokens,
                         parseline_return parseResult, const char *cmdLine,
                         sigset_t *maskAll, sigset_t *maskSelected) {
    pid_t processId;
    sigset_t previousMask;

    // Block specific signals during the execution
    sigprocmask(SIG_BLOCK, maskSelected, &previousMask);

    // Fork a child process to execute the command
    if ((processId = fork()) == 0) { // Child process
        execute_command(tokens, &previousMask);
    }

    // Parent process
    handle_parent_process(processId, parseResult, cmdLine, &previousMask);

    sigprocmask(SIG_SETMASK, &previousMask, NULL);
}

/**
 * @brief Executes a command in a child process.
 *
 * This function is responsible for executing an external command in a child
 * process. It handles I/O redirection, sets the child process in a new process
 * group, and executes the command. If the execution fails, it prints an
 * appropriate error message.
 *
 * @param tokens Struct containing parsed command line tokens.
 * @param previousMask A pointer to the signal set used for unblocking signals
 * after fork.
 */
void execute_command(const struct cmdline_tokens tokens,
                     sigset_t *previousMask) {
    setpgid(0, 0); // Put child in a new process group
    sigprocmask(SIG_SETMASK, previousMask, NULL); // Unblock signals

    // Handle I/O redirection
    redirect_io(tokens.infile, STDIN_FILENO);
    redirect_io(tokens.outfile, STDOUT_FILENO);

    if (execve(tokens.argv[0], tokens.argv, environ) < 0) {
        perror("execve error");
        exit(1);
    }
}

/**
 * @brief Handles the parent process after forking a child.
 *
 * This function is called in the parent process after forking a child to
 * execute a command. Depending on whether the command is to be run in the
 * foreground or background, it adds the job to the job list and either waits
 * for the foreground job to finish or prints information about the background
 * job.
 *
 * @param processId The process ID of the child process.
 * @param parseResult The result of the command line parsing, indicating
 * foreground or background.
 * @param cmdLine The original command line string.
 * @param previousMask A pointer to the signal set used for unblocking signals
 * after fork.
 */
void handle_parent_process(pid_t processId, parseline_return parseResult,
                           const char *cmdLine, sigset_t *previousMask) {
    if (parseResult == PARSELINE_FG) {
        // Add foreground job
        add_job(processId, FG, cmdLine);
        // Wait for the foreground job to finish
        while (fg_job()) {
            sigsuspend(previousMask);
        }
    } else {
        // Add background job
        add_job(processId, BG, cmdLine);
        jid_t jobId = job_from_pid(processId);
        printf("[%d] (%d) %s\n", jobId, processId, cmdLine);
    }
    sigprocmask(SIG_SETMASK, previousMask, NULL);
}

/**
 * Handles redirection of input/output for a command.
 *
 * Redirects the standard input or output of a process to/from a file.
 *
 * @param fileName The name of the file to redirect input/output to/from. If
 * NULL, no redirection is performed.
 * @param fileDescriptor The file descriptor to be redirected (STDIN_FILENO or
 * STDOUT_FILENO).
 */
void redirect_io(const char *fileName, int fileDescriptor) {
    if (fileName != NULL) {
        int fd;
        if (fileDescriptor == STDIN_FILENO) {
            fd = open(fileName, O_RDONLY);
        } else {
            fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        }
        if (fd < 0) {
            perror(fileName);
            exit(1);
        }
        dup2(fd, fileDescriptor);
        close(fd);
    }
}
