#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/wait.h>

#define LOG(msg) printf("\033[1;33m[MALLET]\033[0m %s\n", msg)
#define RUN(argv) do { \
    pid_t p = fork(); \
    if (p == 0) { execvp((argv)[0], (char*const*)(argv)); exit(127); } \
    int s; waitpid(p, &s, 0); \
    if (WEXITSTATUS(s)) exit(WEXITSTATUS(s)); \
} while(0)

static char* getpass_hidden(const char *prompt) {
    printf("%s", prompt);
    fflush(stdout);
    struct termios old, newt;
    tcgetattr(0, &old);
    newt = old; newt.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &newt);
    static char buf[256];
    fgets(buf, sizeof(buf), stdin);
    buf[strcspn(buf, "\n")] = 0;
    tcsetattr(0, TCSANOW, &old);
    printf("\n");
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        LOG("Usage: mallet <dracut options> | systemctl ... | wifi ... | dotfiles ... | --go ...");
        return 1;
    }

    // Dracut direct (any args starting with - are passed to dracut)
    if (argv[1][0] == '-') {
        LOG("Forging initramfs...");
        char **newargv = (char **)malloc((argc + 2) * sizeof(char*));
        newargv[0] = (char*)"sudo"; newargv[1] = (char*)"dracut";
        memcpy(newargv + 2, argv + 1, (argc - 1) * sizeof(char*));
        newargv[argc + 1] = NULL;
        RUN(newargv);
        free(newargv);
    }

    char *cmd = argv[1];

    if (!strcmp(cmd, "systemctl") && argc >= 3) {
        LOG("Striking systemctl...");
        char **newargv = (char **)malloc((argc + 1) * sizeof(char*));
        newargv[0] = (char*)"sudo"; newargv[1] = (char*)"systemctl";
        memcpy(newargv + 2, argv + 2, (argc - 2) * sizeof(char*));
        newargv[argc] = NULL;
        RUN(newargv);
        free(newargv);
    } else if (!strcmp(cmd, "cfdisk") || !strcmp(cmd, "parted")) {
        LOG("Partitioning...");
        char **newargv = (char **)malloc(argc * sizeof(char*));
        newargv[0] = (char*)"sudo";
        memcpy(newargv + 1, argv + 1, (argc - 1) * sizeof(char*));
        newargv[argc] = NULL;
        RUN(newargv);
        free(newargv);
    } else if (!strcmp(cmd, "wifi")) {
        if (argc < 3) {
            LOG("wifi list | connect <ssid>");
            return 1;
        }
        if (!strcmp(argv[2], "list")) {
            LOG("Scanning (wlan0)...");
            const char *list_argv[] = {"iw", "wlan0", "scan", NULL};
            RUN(list_argv);
        } else if (!strcmp(argv[2], "connect") && argc == 4) {
            char *ssid = argv[3];
            char *pass = getpass_hidden("[MALLET] WiFi password: ");
            LOG("Connecting to SSID...");
            // Generate temp conf and run wpa_supplicant + dhcpcd
            char conf[] = "/tmp/wpa.conf";
            FILE *f = fopen(conf, "w");
            fprintf(f, "network={\n  ssid=\"%s\"\n  psk=\"%s\"\n}\n", ssid, pass);
            fclose(f);
            const char *kill_argv[] = {"sudo", "killall", "wpa_supplicant", NULL};
            RUN(kill_argv); // clean
            const char *wpa_argv[] = {"sudo", "wpa_supplicant", "-B", "-i", "wlan0", "-c", conf, NULL};
            RUN(wpa_argv);
            const char *dhcp_argv[] = {"sudo", "dhcpcd", "wlan0", NULL};
            RUN(dhcp_argv);
            remove(conf);
            LOG("Connected");
        }
    } else if (!strcmp(cmd, "dotfiles") && argc == 3) {
        LOG("Installing dotfiles...");
        char syscmd[1024];
        snprintf(syscmd, sizeof(syscmd),
            "mkdir -p ~/.dotfiles && "
            "cd ~/.dotfiles && "
            "git init >/dev/null 2>&1 && "
            "git remote add origin %s && "
            "git fetch --depth 1 && "
            "git checkout -f origin/main && "
            "stow -v .", argv[2]);
        system(syscmd);
    } else if (!strcmp(cmd, "--go") && argc == 3) {
        LOG("Igniting service...");
        const char *go_argv[] = {"sudo", "systemctl", "enable", "--now", argv[2], NULL};
        RUN(go_argv);
    } else {
        LOG("Invalid command");
        return 1;
    }

    LOG("Steel is forged");
    return 0;
}
