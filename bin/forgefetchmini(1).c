// forgefetch.c — Neofetch-style output with dynamic spacing and test flag
// Build: gcc -O2 -Wall -o forgefetch forgefetch.c
// Run:   ./forgefetch
// Test:  ./forgefetch --archforged

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <stddef.h>

// ANSI colors
#define C_RED  "\033[91m"
#define C_GRN  "\033[32m"
#define C_YEL  "\033[33m"
#define C_WHT  "\033[97m"
#define C_RST  "\033[0m"

static inline void now(struct timespec* t) { clock_gettime(CLOCK_MONOTONIC, t); }
static inline double ms(const struct timespec* s, const struct timespec* e) {
    return (e->tv_sec - s->tv_sec) * 1000.0 + (e->tv_nsec - s->tv_nsec) / 1000000.0;
}

// Global distro name and flag
static char pretty_name[128] = "Unknown";
static int is_archforged = 0;

// ------------------ System Info Helpers ------------------

void get_os_name(char* out, size_t outsz) {
    FILE* f = fopen("/etc/os-release", "r");
    if(!f) { strncpy(out, "Unknown", outsz-1); out[outsz-1]='\0'; return; }
    char line[256];
    while(fgets(line, sizeof(line), f)) {
        if(strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char* val = strchr(line, '=');
            if(val) {
                val++;
                if(val[0] == '"') val++;
                char* nl = strchr(val, '\n');
                if(nl) *nl = '\0';
                strncpy(out, val, outsz-1);
                out[outsz-1] = '\0';
                strncpy(pretty_name, out, sizeof(pretty_name)-1);
                pretty_name[sizeof(pretty_name)-1] = '\0';
                if(strcmp(pretty_name, "Archforged") == 0) is_archforged = 1;
                fclose(f);
                return;
            }
        }
    }
    fclose(f);
    strncpy(out, "Unknown", outsz-1);
    out[outsz-1]='\0';
    strncpy(pretty_name, out, sizeof(pretty_name)-1);
    pretty_name[sizeof(pretty_name)-1]='\0';
}

void get_kernel(char* out, size_t outsz) {
    struct utsname u;
    if(uname(&u) == 0)
        snprintf(out, outsz, "%s %s", u.sysname, u.release);
    else { strncpy(out, "Unknown", outsz-1); out[outsz-1]='\0'; }
}

void get_uptime(char* out, size_t outsz) {
    FILE* f = fopen("/proc/uptime", "r");
    if(!f) { strncpy(out, "Unknown", outsz-1); out[outsz-1]='\0'; return; }
    double sec=0; fscanf(f, "%lf", &sec); fclose(f);
    long s=(long)sec; long d=s/86400; s%=86400; long h=s/3600; s%=3600; long m=s/60;
    if(d>0) snprintf(out,outsz,"%ldd %ldh %ldm",d,h,m);
    else if(h>0) snprintf(out,outsz,"%ldh %ldm",h,m);
    else snprintf(out,outsz,"%ldm",m);
}

void get_memory(char* out, size_t outsz) {
    FILE* f=fopen("/proc/meminfo","r");
    if(!f){strncpy(out,"Unknown",outsz-1); out[outsz-1]='\0'; return;}
    long total=0,avail=0; char key[64]; long val; char unit[16];
    while(fscanf(f,"%63s %ld %15s",key,&val,unit)==3){
        if(strcmp(key,"MemTotal:")==0) total=val;
        else if(strcmp(key,"MemAvailable:")==0) avail=val;
        int c; while((c=fgetc(f))!='\n'&&c!=EOF){}
        if(total&&avail) break;
    }
    fclose(f);
    double t=total/(1024.0*1024.0), u=(total-avail)/(1024.0*1024.0);
    snprintf(out,outsz,"%.1f GiB / %.1f GiB",u,t);
}

void get_disk(char* out, size_t outsz) {
    struct statvfs s;
    if(statvfs("/",&s)!=0){strncpy(out,"Unknown",outsz-1); out[outsz-1]='\0'; return;}
    unsigned long long total=(unsigned long long)s.f_frsize*s.f_blocks;
    unsigned long long free=(unsigned long long)s.f_frsize*s.f_bfree;
    unsigned long long used=total-free;
    double t=total/(1024.0*1024.0*1024.0), u=used/(1024.0*1024.0*1024.0);
    int pct=(int)((used*100.0)/(double)total+0.5);
    snprintf(out,outsz,"%.1f GiB / %.1f GiB (%d%%)",u,t,pct);
}

int get_package_count(void) {
    // Try pacman; extend with dpkg/rpm as needed
    FILE* pipe=popen("pacman -Q | wc -l 2>/dev/null","r");
    if(!pipe) return 0;
    int count=0; fscanf(pipe,"%d",&count); pclose(pipe);
    return count;
}

// ------------------ Badges ------------------

// Archforged badge (exact ASCII you provided)
const char* archforged_badge[] = {
    "                                      *:                                   ",
    "                                     **+.                                  ",
    "                                    -***=                                  ",
    "                                   :*****-                                 ",
    "                                   ******+:                                ",
    "                                  +*******=.                               ",
    "                                 =********+=.                              ",
    "                                :**********+=                              ",
    "                               .+***********=-                             ",
    "                              .+************+=:                            ",
    "                              :++++++********+=:                           ",
    "                             :  :+++++++++*+**+=:                          ",
    "                            =++=: :++++++++++++==.                         ",
    "                           :++++++=:.=++++++++++==.                        ",
    "                          :===+++++++++++++++++++==                        ",
    "                         :=========+++++++++++++++==                       ",
    "                        :===============+++++++++++==                      ",
    "                       :===========+*************+++=-                     ",
    "                      .=======+***********************=                    ",
    "                     .====+****************************=                   ",
    "                    .==*********************************=                  ",
    "                   .+***************-   :+***************=                 ",
    "                  :***************.     . .+**************-                ",
    "                 .**************+    .%%*%+.=**************-               ",
    "                 ***************.   .*-*=+=* =**************-              ",
    "               .***************: .*#*%*:=:%: :***************-             ",
    "              .****************  *:%=%-*#-.   **********+*****-            ",
    "             .****************+  -%+**+-:+.   =**********+:.:=*-           ",
    "            .*****************+       +===    =*************+:             ",
    "           .*******************               =****************-           ",
    "          .*****************+=-.              -=+****************=.        ",
    "         .*************=:.                         .:=*************:       ",
    "        .*********+-.                                   .:+*********:      ",
    "       .*******-.                                           .:+******:     ",
    "      :****=:                                                   .=****:    ",
    "     :**-.                                                          :**:   ",
    "     :                                                                 :.  ",
    NULL
};

// Compact question-mark badge (~18 lines, ~29 chars wide)
const char* question_badge[] = {
    "            #######            ",
    "          ###########          ",
    "         ####     ####         ",
    "        ####       ####        ",
    "        ####       ####        ",
    "                 ######        ",
    "                ######         ",
    "               ######          ",
    "              ######           ",
    "             ######            ",
    "            ######             ",
    "            ####               ",
    "                               ",
    "            ####               ",
    "            ####               ",
    "            ####               ",
    "                               ",
    NULL
};

// ------------------ Utility for dynamic padding ------------------

static int str_raw_len(const char* s) {
    int n=0; while(s && s[n] != '\0') n++; return n;
}

static int badge_max_width(const char** badge) {
    int maxw = 0;
    for(int i=0; badge[i]!=NULL; i++) {
        int w = str_raw_len(badge[i]);
        if(w > maxw) maxw = w;
    }
    return maxw;
}

// ------------------ Main ------------------

int main(int argc, char* argv[]) {
    struct timespec start,end;
    now(&start);

    // Dummy flag to force Archforged badge for testing
    for(int a=1; a<argc; a++) {
        if(strcmp(argv[a], "--archforged")==0) {
            is_archforged = 1;
            strncpy(pretty_name, "Archforged", sizeof(pretty_name)-1);
            pretty_name[sizeof(pretty_name)-1] = '\0';
        }
    }

    char os[128], kernel[256], uptime[64], mem[64], disk[64];
    get_os_name(os,sizeof(os));
    get_kernel(kernel,sizeof(kernel));
    get_uptime(uptime,sizeof(uptime));
    get_memory(mem,sizeof(mem));
    get_disk(disk,sizeof(disk));
    int pkgs=get_package_count();
    static char pkgline[64];
    snprintf(pkgline,sizeof(pkgline),"%d (pacman)",pkgs);

    // Right-column details (first line is a header)
    const char* details[] = {
        "Forgefetch is hammering out system details…",
        os,
        kernel,
        uptime,
        mem,
        disk,
        pkgline,
        NULL
    };

    // Choose badge
    const char** badge = is_archforged ? archforged_badge : question_badge;

    // Dynamic spacing: compute max lines and dynamic pad width
    int badge_lines = 0, detail_lines = 0;
    while(badge[badge_lines] != NULL) badge_lines++;
    while(details[detail_lines] != NULL) detail_lines++;
    int max_lines = (badge_lines > detail_lines) ? badge_lines : detail_lines;

    int pad = badge_max_width(badge);     // raw width of the widest badge line
    int gap = 3;                          // space between columns

    // Side-by-side printing like Neofetch
    for(int i=0; i<max_lines; i++) {
        // Left column: badge line with selective coloring; then pad to 'pad'
        int printed_len = 0;
        if(i < badge_lines && badge[i]) {
            const char* line = badge[i];
            for(int j=0; line[j] != '\0'; j++) {
                char c = line[j];
                if(c=='*')      { printf(C_RED "%c" C_RST, c); }
                else if(c=='+') { printf(C_YEL "%c" C_RST, c); }
                else if(c=='=') { printf(C_WHT "%c" C_RST, c); }
                else            { printf("%c", c); }
                printed_len++;
            }
        }
        // Pad spaces to align right column after the badge
        while(printed_len < pad) { putchar(' '); printed_len++; }

        // Gap between columns
        for(int g=0; g<gap; g++) putchar(' ');

        // Right column: details
        if(i < detail_lines && details[i]) {
            if(i==1) printf(C_GRN "Distro:    " C_RST "%s\n", details[i]);
            else if(i==2) printf(C_GRN "Kernel:    " C_RST "%s\n", details[i]);
            else if(i==3) printf(C_GRN "Uptime:    " C_RST "%s\n", details[i]);
            else if(i==4) printf(C_GRN "Memory:    " C_RST "%s\n", details[i]);
            else if(i==5) printf(C_GRN "Disk:      " C_RST "%s\n", details[i]);
            else if(i==6) printf(C_GRN "Packages:  " C_RST "%s\n", details[i]);
            else          printf("%s\n", details[i]);
        } else {
            putchar('\n');
        }
    }

    now(&end);
    printf(C_YEL "Forgefetch completed in %.3f ms\n" C_RST, ms(&start,&end));
    return 0;
}
