#include <stdlib.h>
#include "id.h"

int uidbyname(const char *name, uid_t *uid)
{
    char *end = NULL;
    struct passwd *pwd;

    *uid = strtol(name, &end, 10);
    if (*uid >= 0 && !*end) return 1;

    if (!(pwd = getpwnam(name))) return 0;
    *uid = pwd->pw_uid;
    return 1;
}

int gidbyname(const char *name, gid_t *gid)
{
    char *end = NULL;
    struct group *grp;

    *gid = strtol(name, &end, 10);
    if (*gid >= 0 && !*end) return 1;

    if (!(grp = getgrnam(name))) return 0;
    *gid = grp->gr_gid;
    return 1;
}

