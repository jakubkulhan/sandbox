#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "help.h"
#include "socket.h"
#include "id.h"

static int new_environ = 0, forks = 0;
static char *in = NULL, *out = NULL, *err = NULL, *root = NULL, *user = NULL, *group = NULL;
static void parse_params(int argc, char **argv)
{
    int opt;

    opterr = optind = 0;
    while ((opt = getopt(argc, argv, "0:1:2:r:f:hu:g:e")) != -1) {
        switch (opt) {
            case '0': in = optarg;              break;
            case '1': out = optarg;             break;
            case '2': err = optarg;             break;
            case 'r': root = optarg;            break;
            case 'f': forks = atoi(optarg);     break;
            case 'h': help(argv[0]); exit(0);   break;
            case 'u': user = optarg;            break;
            case 'g': group = optarg;           break;
            case 'e': new_environ = 1;          break;
            default : usage(argv[0]); exit(-1);
        }
    }
}

static void check_uid(void)
{
    if (getuid() != 0) {
        fprintf(stderr, "init: must be run as root\n");
        exit(-1);
    }
}

extern char **environ;
static char *dummy_environ[] = { NULL };
static void clear_environ(void)
{
    environ = dummy_environ;
}

static void init_environ(int argc, char **argv)
{
    int i;
    for (i = optind; i < argc; ++i) {
        if (!strchr(argv[i], '=')) break;
        putenv(argv[i]);
    }
    optind = i;
}

static char *another_prog = NULL;
static int another_argc = 1;
static char *another_argv[16] = { "", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static void init_args(int argc, char **argv)
{
    int i, j;

    if (optind >= argc) {
        fprintf(stderr, "init: what? no program?!\n");
        exit(-1);
    }

    another_prog = argv[optind++];

    for (i = optind, j = 1; i < argc && j < 16; ++i, ++j) {
        another_argv[j] = argv[i];
        ++another_argc;
    }
}

static int (*another_main)(int argc, char *argv[]);
static void *load_another_main(void)
{
    void *handle;
    char *error;
    
    if ((handle = dlopen(another_prog, RTLD_NOW)) == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        exit(-1);
    }

    dlerror();
    *(void **)(&another_main) = dlsym(handle, "main");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        dlclose(handle);
        exit(-1);
    }

    return handle;
}

static int in_des = -1, out_des = -1, err_des = -1;
#define IS_BAD_SOCKET_T(s, str) \
    if ((s).type == T_BAD) { \
        fprintf(stderr, "bad socket %s\n", str); \
        exit(-1); \
    }

#define OPEN_SOCKET_T(des, s) \
    if (((des) = open_socket((s), &errstr)) == -1) { \
        fprintf(stderr, "open_socket: %s\n", errstr); \
        exit(-1); \
    }

#define INIT_ONE_DES(str, des) \
    if (str) { \
        s = parse_socket(str); \
        IS_BAD_SOCKET_T(s, str); \
        OPEN_SOCKET_T(des, s); \
    }
static void init_des(void)
{
    struct socket_t s;
    char *errstr;

    INIT_ONE_DES(in, in_des);
    INIT_ONE_DES(out, out_des);
    INIT_ONE_DES(err, err_des);
}

static int drop_user_priv(uid_t new_uid)
{
    uid_t ruid, euid, suid;
    if (setresuid(new_uid, new_uid, new_uid) < 0) return 0;
    if (getresuid(&ruid, &euid, &suid) < 0) return 0;
    if (ruid != new_uid || euid != new_uid || suid != new_uid) return 0;
    return 1;
}

static int drop_group_priv(gid_t new_gid)
{
    gid_t rgid, egid, sgid;
    if (setresgid(new_gid, new_gid, new_gid) < 0) return 0;
    if (getresgid(&rgid, &egid, &sgid) < 0) return 0;
    if (rgid != new_gid || egid != new_gid || sgid != new_gid) return 0;
    return 1;
}

#define CHILD_ERR_UPRIV 128
#define CHILD_ERR_GPRIV 129
#define CHILD_ERR_CHROOT 130
#define CHILD_ERR_STDIN 131
#define CHILD_ERR_STDOUT 132
#define CHILD_ERR_STDERR 133

static int spawn(void)
{
    void *handle = load_another_main();
    pid_t pid;
    uid_t uid = -1;
    gid_t gid = -1;
    struct timeval tv = { 0, 100 * 1000 };
    int status;

    if (user && !uidbyname(user, &uid)) {
        fprintf(stderr, "uidbyname: %s\n", strerror(errno));
        return -1;
    }

    if (group && !gidbyname(group, &gid)) {
        fprintf(stderr, "gidbyname: %s\n", strerror(errno));
        return -1;
    }

    do {
        pid = fork();
        if (pid == 0) {
            if (in_des != -1 && dup2(in_des, STDIN_FILENO) == -1) return CHILD_ERR_STDIN;
            if (out_des != -1 && dup2(out_des, STDOUT_FILENO) == -1) return CHILD_ERR_STDOUT;
            if (err_des != -1 && dup2(err_des, STDERR_FILENO) == -1) return CHILD_ERR_STDERR;
            if (root && !(chdir(root) != -1 && chroot(root) != -1)) return CHILD_ERR_CHROOT;
            if (gid != -1 && !drop_group_priv(gid)) return CHILD_ERR_GPRIV;
            if (uid != -1 && !drop_user_priv(uid)) return CHILD_ERR_UPRIV;

            return another_main(another_argc, another_argv);

        } else if (pid == -1) {
            fprintf(stderr, "fork: %s\n", strerror(errno));
            dlclose(handle);
            return -1;
        }

        select(0, NULL, NULL, NULL, &tv); /* wait 100ms */
        switch (waitpid(pid, &status, WNOHANG)) {
            case 0:
                fprintf(stdout, "child %d\n", pid);
            break;

            case -1: break;

            default:
                if (WIFEXITED(status)) {
                    switch (WEXITSTATUS(status)) {
                        case CHILD_ERR_UPRIV:
                            fprintf(stderr, "spawn: child could not change UID\n");
                        break;

                        case CHILD_ERR_GPRIV:
                            fprintf(stderr, "spawn: child could not change GID\n");
                        break;

                        case CHILD_ERR_CHROOT:
                            fprintf(stderr, "spawn: child could not chroot\n");
                        break;

                        case CHILD_ERR_STDIN:
                            fprintf(stderr, "spawn: child could not redirect stdin\n");
                        break;

                        case CHILD_ERR_STDOUT:
                            fprintf(stderr, "spawn: child could not redirect stdout\n");
                        break;

                        case CHILD_ERR_STDERR:
                            fprintf(stderr, "spawn: child could not redirect stderr\n");
                        break;

                        default:
                            fprintf(stderr, "spawn: child died with status %d\n", WEXITSTATUS(status));
                    }
                } else if (WIFSIGNALED(status)) {
                    fprintf(stderr, "spawn: child signaled %d\n", WTERMSIG(status));
                } else {
                    fprintf(stderr, "spawn: child died\n");
                }
        }
    } while(--forks > 0);

    dlclose(handle);
    return 0;
}

int main(int argc, char **argv)
{
    parse_params(argc, argv);
    check_uid();
    if (new_environ) clear_environ();
    init_environ(argc, argv);
    init_args(argc, argv);
    init_des();

    return spawn();
}
