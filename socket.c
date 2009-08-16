#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>

#include "socket.h"
#include "id.h"

struct file_socket {
    char path[108];
    char user[32];
    char group[32];
    int mode;
};

static int parse_file_socket(const char *str, struct socket_t *s)
{
    char *start, *end, *path, *comma;
    path = (char *) &(str[1]);

    if (*path == '[') {
        start = path + 1;
        if (!(end = strchr(start, ','))) return 0;
        strncpy(s->socket.s_file.user, start, end - start);

        start = end + 1;
        if (!(end = strchr(start, ']'))) return 0;

        comma = strchr(start, ',');
        if (comma != NULL && comma < end) {
            sscanf(comma + 1, "%o", &s->socket.s_file.mode);
            end = end;
        } else comma = end;

        strncpy(s->socket.s_file.group, start, comma - start);

        path = end + 1;
    }

    strcpy(s->socket.s_file.path, path);
    return 1;
}

struct socket_t parse_socket(const char *str)
{
    struct socket_t s;
    char *start, *end;

    memset(&s, 0, sizeof(s));
    s.type = T_BAD;

    if (str[0] == '@') {
        if (!parse_file_socket(str, &s)) return s;
        s.type = T_UNIX;

    } else if (str[0] == ':') {
        if (!parse_file_socket(str, &s)) return s;
        s.type = T_FILE;

    } else {
        if (str[0] == '[') { /* IPv6 */
            if (!(start = strrchr(str, ']'))) return s;
            end = (char *) str + strlen(str) - 1;
            if (start + 2 < str + strlen(str)) {
                strcpy(s.socket.s_inet.port, start + 2);
                end = start;
            }

            start = (char *) &(str[1]);
            strncpy(s.socket.s_inet.addr, start, end - start);
        } else { /* IPv4 */
            end = (char *) str + strlen(str);
            if ((start = strchr(str, ':'))) {
                strcpy(s.socket.s_inet.port, start + 1);
                end = start;
            }
            strncpy(s.socket.s_inet.addr, str, end - str);
        }

        s.type = T_INET;
    }

    return s;
}

#define ON_ERR_CLEANUP(exp, cleanup) \
    do { \
        if (exp) { \
            *err = strerror(errno); \
            cleanup \
            return -1; \
        } \
    } while(0)

#define ON_ERR_NOCLEANUP(exp) ON_ERR_CLEANUP(exp, {})

int open_socket(struct socket_t s, char **err)
{
    int ret, fd, yes = 1;
    struct addrinfo hints, *res, *i;
    struct sockaddr_un addr;
    uid_t uid;
    gid_t gid;

    memset(&hints, 0, sizeof(hints));
    *err = NULL;

    if (s.type == T_INET) {
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if ((ret = getaddrinfo(s.socket.s_inet.addr, s.socket.s_inet.port, &hints, &res)) != 0) {
            *err = (char *) gai_strerror(ret);
            return -1;
        }

        for (i = res; i != NULL; i = i->ai_next) {
            if ((fd = socket(i->ai_family, i->ai_socktype, i->ai_protocol)) == -1) continue;

            ON_ERR_CLEANUP(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1, {
                close(fd);
                freeaddrinfo(res);
            });

            if (bind(fd, i->ai_addr, i->ai_addrlen) == -1) {
                close(fd);
                continue;
            }

            break;
        }

        freeaddrinfo(res);
        if (i == NULL) return -1;

        ON_ERR_CLEANUP(listen(fd, 50) == -1, { close(fd); });
        return fd;

    } else if (s.type == T_UNIX) {

        ON_ERR_NOCLEANUP((fd = socket(AF_UNIX, SOCK_STREAM, 0) == -1));
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, s.socket.s_unix.path);

        ON_ERR_NOCLEANUP(unlink(addr.sun_path) == -1 && errno != ENOENT);
        ON_ERR_CLEANUP(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1, 
            { close(fd); });

        ON_ERR_CLEANUP(bind(fd, (struct sockaddr *) &addr, sizeof(addr.sun_family) + 
            strlen(addr.sun_path)) == -1, { close(fd); });

        ON_ERR_CLEANUP(listen(fd, 50) == -1, { close(fd); unlink(addr.sun_path); });

        if (strlen(s.socket.s_unix.user) > 0) {
            ON_ERR_CLEANUP(!uidbyname(s.socket.s_unix.user, &uid) ||
                !gidbyname(s.socket.s_unix.group, &gid), { close(fd); 
                unlink(addr.sun_path); });

            ON_ERR_CLEANUP(chown(addr.sun_path, uid, gid) == -1, { close(fd);
                unlink(addr.sun_path); });
        }

        if (s.socket.s_unix.mode != 0) {
            ON_ERR_CLEANUP(chmod(addr.sun_path, s.socket.s_unix.mode) == -1, {
                close(fd); unlink(addr.sun_path); });
        }

        return fd;

    } else if (s.type == T_FILE) {
        ON_ERR_NOCLEANUP((fd = open(s.socket.s_file.path, O_RDWR | O_CREAT | O_APPEND, 
                        s.socket.s_file.mode != 0 ? s.socket.s_file.mode : 0644)) == -1);

        if (strlen(s.socket.s_file.user) > 0) {
            ON_ERR_CLEANUP(!uidbyname(s.socket.s_file.user, &uid) || 
                !gidbyname(s.socket.s_file.group, &gid), { close(fd); });

            ON_ERR_CLEANUP(chown(s.socket.s_file.path, uid, gid) == -1, { close(fd); });
        }

        if (s.socket.s_file.mode != 0) {
            ON_ERR_CLEANUP(chmod(s.socket.s_file.path, s.socket.s_file.mode) == -1, 
                { close(fd); });
        }

        return fd;
    }

    *err = "bad socket type";
    return -1;
}
