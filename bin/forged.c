#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define LOG(msg) printf("\033[1;35m[FORGED]\033[0m %s\n", msg)
#define RUN(argv) do { \
    pid_t p = fork(); \
    if (p == 0) { execvp(argv[0], (char *const *)argv); exit(1); } \
    else { int s; waitpid(p, &s, 0); if (WEXITSTATUS(s)) exit(1); } \
} while (0)

int dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && !S_ISDIR(st.st_mode);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        LOG("Usage: forged [-B pkg] [-G repo] [-GR name] [-R pkg]");
        return 1;
    }

    char *op = argv[1];

    if (!strcmp(op, "-B")) {
        char *pkg = argv[2];
        LOG("Searching smithies...");
        FILE *fp = popen("pacman -Ss ^${pkg}$", "r");
        char official[1024] = {0};
        if (fp) fgets(official, sizeof(official), fp);
        pclose(fp);
        char *official_pkg = strtok(official, " ");

        fp = popen("yay -Ssa ${pkg}", "r");
        char aur[1024] = {0};
        if (fp) fgets(aur, sizeof(aur), fp);
        pclose(fp);
        char *aur_pkg = strtok(strstr(aur, "/") ? strstr(aur, "/") + 1 : aur, " ");

        if (!official_pkg && !aur_pkg) {
            LOG("No smithy found");
            return 1;
        }

        printf("[FORGED] Found in %s smithy(ies):\n", official_pkg && aur_pkg ? "two" : "one");
        int count = 1;
        if (official_pkg) printf("  [%d] %s - official repos\n", count++, official_pkg);
        if (aur_pkg) printf("  [%d] %s - AUR\n", count, aur_pkg);

        char choice[16];
        printf("[FORGED] Pick a source (ENTER = official): ");
        fgets(choice, sizeof(choice), stdin);
        char *chosen = (choice[0] == '\n' && official_pkg) ? official_pkg : (choice[0] == '2' ? aur_pkg : official_pkg);

        LOG("Installing...");
        const char *cmd[] = {"yay", "-S", "--noconfirm", "--needed", chosen, NULL};
        RUN(cmd);
        LOG("Installed");
    } else if (!strcmp(op, "-G")) {
        for (int i = 2; i < argc; i++) {
            char *target = argv[i];
            char name[256];
            strcpy(name, strrchr(target, '/') ? strrchr(target, '/') + 1 : target);
            char *dotgit = strstr(name, ".git");
            if (dotgit) *dotgit = 0;

            LOG("Forging from source...");
            system("mkdir -p ~/forged/git");
            chdir("~/forged/git");
            char rmcmd[512];
            snprintf(rmcmd, sizeof(rmcmd), "rm -rf %s", name);
            system(rmcmd);
            const char *clone[] = {"git", "clone", "--depth=1", target, name, NULL};
            RUN(clone);
            chdir(name);

            if (file_exists("meson.build")) {
                LOG("Meson detected");
                const char *meson[] = {"meson", "setup", "build", NULL};
                RUN(meson);
                chdir("build");
                const char *ninja[] = {"ninja", NULL};
                RUN(ninja);
                const char *install[] = {"sudo", "ninja", "install", NULL};
                RUN(install);
            } else if (file_exists("CMakeLists.txt")) {
                LOG("CMake detected");
                const char *cmake[] = {"cmake", "-Bbuild", "-DCMAKE_INSTALL_PREFIX=/usr", NULL};
                RUN(cmake);
                const char *build[] = {"cmake", "--build", "build", "-j$(nproc)", NULL};
                RUN(build);
                const char *install[] = {"sudo", "cmake", "--install", "build", NULL};
                RUN(install);
            } else if (file_exists("Makefile") || file_exists("makefile")) {
                LOG("Make detected");
                const char *make[] = {"make", "-j$(nproc)", NULL};
                RUN(make);
                const char *install[] = {"sudo", "make", "install", NULL};
                RUN(install);
            } else if (file_exists("PKGBUILD")) {
                LOG("PKGBUILD detected");
                const char *makepkg[] = {"makepkg", "-si", "--noconfirm", NULL};
                RUN(makepkg);
            } else {
                LOG("No build system - shell");
                const char *bash[] = {"bash", NULL};
                RUN(bash);
            }
            LOG("Forged from source");
        }
    } else if (!strcmp(op, "-GR")) {
        char *name = argv[2];
        char path[512];
        snprintf(path, sizeof(path), "~/forged/git/%s", name);
        if (!dir_exists(path)) {
            LOG("Nothing forged");
            return 1;
        }
        LOG("Smelting...");
        char rmcmd[512];
        snprintf(rmcmd, sizeof(rmcmd), "rm -rf %s", path);
        system(rmcmd);
        LOG("Removed");
    } else if (!strcmp(op, "-R")) {
        LOG("Removing...");
        char **cmd = malloc((argc + 5) * sizeof(char*));
        cmd[0] = "sudo"; cmd[1] = "pacman"; cmd[2] = "-Rns"; cmd[3] = "--noconfirm";
        for (int i = 2; i < argc; i++) cmd[i + 2] = argv[i];
        cmd[argc + 2] = NULL;
        RUN((const char **)cmd);
        free(cmd);
        LOG("Removed");
    } else {
        // Forward other flags to yay
        LOG("Forwarding to yay...");
        char **cmd = malloc((argc + 1) * sizeof(char*));
        cmd[0] = "yay";
        for (int i = 1; i < argc; i++) cmd[i] = argv[i];
        cmd[argc] = NULL;
        RUN((const char **)cmd);
        free(cmd);
    }

    LOG("Steel is tempered");
    return 0;
}
