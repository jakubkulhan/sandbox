#ifndef SOCKET_H
#define SOCKET_H

#include <netdb.h> /* for INET6_ADDRSTRLEN */

struct socket_t {
    enum { T_BAD, T_INET, T_UNIX, T_FILE } type;
    union {
        struct {
            char addr[INET6_ADDRSTRLEN];
            char port[6];
        } s_inet;

        struct {
            char path[108];
            char user[32];
            char group[32];
            unsigned int mode;
        } s_unix;

        struct {
            char path[108];
            char user[32];
            char group[32];
            unsigned int mode;
        } s_file;
    } socket;
};

struct socket_t parse_socket(const char *str);

int open_socket(struct socket_t s, char **err);

#endif /* socket.h */
