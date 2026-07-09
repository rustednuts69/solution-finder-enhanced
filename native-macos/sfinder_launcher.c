#include <errno.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int executable_dir(char *buffer, size_t size) {
    uint32_t path_size = (uint32_t)size;
    if (_NSGetExecutablePath(buffer, &path_size) != 0) {
        fprintf(stderr, "Executable path is too long\n");
        return -1;
    }

    char resolved[PATH_MAX];
    if (realpath(buffer, resolved) == NULL) {
        perror("realpath");
        return -1;
    }

    char *slash = strrchr(resolved, '/');
    if (slash == NULL) {
        fprintf(stderr, "Could not resolve executable directory\n");
        return -1;
    }
    *slash = '\0';

    if (strlen(resolved) + 1 > size) {
        fprintf(stderr, "Executable directory is too long\n");
        return -1;
    }
    strcpy(buffer, resolved);
    return 0;
}

static int parent_dir(char *path) {
    char *slash = strrchr(path, '/');
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';
    return 0;
}

int main(int argc, char **argv) {
    char bin_dir[PATH_MAX];
    if (executable_dir(bin_dir, sizeof(bin_dir)) != 0) {
        return 1;
    }

    char root_dir[PATH_MAX];
    strcpy(root_dir, bin_dir);
    if (parent_dir(root_dir) != 0) {
        fprintf(stderr, "Could not resolve launcher root\n");
        return 1;
    }

    char classpath[PATH_MAX * 2];
    int written = snprintf(
        classpath,
        sizeof(classpath),
        "%s/lib/sfinder-from-source.jar:%s/lib/sfinder-deps.jar",
        root_dir,
        root_dir
    );
    if (written < 0 || (size_t)written >= sizeof(classpath)) {
        fprintf(stderr, "Classpath is too long\n");
        return 1;
    }

    char **java_argv = calloc((size_t)argc + 4, sizeof(char *));
    if (java_argv == NULL) {
        perror("calloc");
        return 1;
    }

    java_argv[0] = "java";
    java_argv[1] = "-cp";
    java_argv[2] = classpath;
    java_argv[3] = "Main";
    for (int i = 1; i < argc; i++) {
        java_argv[i + 3] = argv[i];
    }
    java_argv[argc + 3] = NULL;

    execvp("java", java_argv);

    if (errno == ENOENT) {
        fprintf(stderr, "Java was not found. Install a JRE/JDK to run sfinder.\n");
    } else {
        perror("execvp java");
    }
    free(java_argv);
    return 127;
}
