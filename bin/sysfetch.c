#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define LOG(msg) printf("\033[1;35m[SYSFETCH]\033[0m %s\n", msg)
#define REPO "https://raw.githubusercontent.com/Archforged/archforged_meta/main/bin/"

struct Memory { char *data; size_t size; };

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    return realsize;
}

int download(const char *url, const char *outfile) {
    CURL *curl = curl_easy_init();
    if (!curl) return 1;
    struct Memory chunk = {0};
    FILE *fp = fopen(outfile, "wb");
    if (!fp) { curl_easy_cleanup(curl); return 1; }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) fwrite(chunk.data, 1, chunk.size, fp);
    free(chunk.data);
    fclose(fp);
    curl_easy_cleanup(curl);
    return res != CURLE_OK;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        LOG("Usage: sysfetch get <groupname> | remove <groupname>");
        return 1;
    }
    char *cmd = argv[1];
    char *group = argv[2];
    if (!strcmp(cmd, "get")) {
        if (!strcmp(group, "archforged_meta")) {
            LOG("Fetching archforged_meta tools...");
            char *files[] = {"forgefetch", "forged", "mallet", "forgetools.sh", NULL};
            for (int i = 0; files[i]; i++) {
                char url[256], outfile[256];
                snprintf(url, sizeof(url), "%s%s", REPO, files[i]);
                snprintf(outfile, sizeof(outfile), "./%s", files[i]);
                if (download(url, outfile)) {
                    LOG("Fetch failed");
                    return 1;
                }
                LOG("Fetched");
            }
        } else {
            LOG("Fetching pacman group...");
            char syscmd[256];
            snprintf(syscmd, sizeof(syscmd), "sudo pacman -S --needed %s", group);
            system(syscmd);
        }
    } else if (!strcmp(cmd, "remove")) {
        LOG("Removing group...");
        char syscmd[256];
        snprintf(syscmd, sizeof(syscmd), "sudo pacman -Rns $(pacman -Qgq %s)", group);
        system(syscmd);
    } else {
        LOG("Invalid command");
        return 1;
    }
    LOG("Done");
    return 0;
}
