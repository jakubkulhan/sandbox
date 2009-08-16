#ifndef ID_H
#define ID_H

#include <pwd.h>
#include <grp.h>

int uidbyname(const char *name, uid_t *uid);

int gidbyname(const char *name, gid_t *gid);

#endif /* id.h */
