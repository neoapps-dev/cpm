#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <curl/curl.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef DT_REG
#define DT_REG 8
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#define CPM_DIR ".cpm"
#define CPM_FILE "cpmfile"
#define CPM_VERSION "0.1.0"
#define MAKEFILE "Makefile"
#define GITIGNORE ".gitignore"
#define URL_BASE "https://raw.githubusercontent.com/neoapps-dev/cpm-repo/refs/heads/main//"
typedef struct {
    char name[256];
    char version[64];
    char url[512];
} Package;
typedef struct {
    size_t size;
    char *data;
} MemoryStruct;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    
    return realsize;
}

int ensure_directory_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0700) == -1) {
            fprintf(stderr, "Failed to create directory %s\n", path);
            return 0;
        }
    }
    return 1;
}

void ensure_gitignore_updated() {
    FILE *fp;
    char line[256];
    int cpm_found = 0;
    fp = fopen(GITIGNORE, "r");
    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, CPM_DIR)) {
                cpm_found = 1;
                break;
            }
        }
        fclose(fp);
    }
    
    if (!cpm_found) {
        fp = fopen(GITIGNORE, "a+");
        if (fp) {
            fprintf(fp, "\n%s/\n", CPM_DIR);
            fclose(fp);
            printf("Updated %s to ignore %s directory\n", GITIGNORE, CPM_DIR);
        } else {
            fp = fopen(GITIGNORE, "w");
            if (fp) {
                fprintf(fp, "%s/\n", CPM_DIR);
                fclose(fp);
                printf("Created %s to ignore %s directory\n", GITIGNORE, CPM_DIR);
            } else {
                fprintf(stderr, "Failed to update %s\n", GITIGNORE);
            }
        }
    }
}

int download_package(const char *package_name, const char *version) {
    CURL *curl;
    CURLcode res;
    MemoryStruct chunk;
    char package_url[512];
    char package_dir[512];
    char package_file[512];
    FILE *fp;
    chunk.data = malloc(1);
    chunk.size = 0;
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Failed to initialize curl\n");
        free(chunk.data);
        return 0;
    }
    
    snprintf(package_url, sizeof(package_url), "%s%s/%s.tar.gz", URL_BASE, package_name, version);
    snprintf(package_dir, sizeof(package_dir), "%s/%s", CPM_DIR, package_name);
    snprintf(package_file, sizeof(package_file), "%s/%s-%s.tar.gz", CPM_DIR, package_name, version);
    if (!ensure_directory_exists(package_dir)) {
        curl_easy_cleanup(curl);
        free(chunk.data);
        return 0;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, package_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "cpm/" CPM_VERSION);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);    
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to download package %s: %s\n", package_name, curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(chunk.data);
        return 0;
    }
    
    fp = fopen(package_file, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to create package file %s\n", package_file);
        curl_easy_cleanup(curl);
        free(chunk.data);
        return 0;
    }
    
    fwrite(chunk.data, 1, chunk.size, fp);
    fclose(fp);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "tar -xzf %s -C %s", package_file, package_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to extract package %s\n", package_name);
        curl_easy_cleanup(curl);
        free(chunk.data);
        return 0;
    }
    
    curl_easy_cleanup(curl);
    free(chunk.data);
    printf("Installed package %s version %s\n", package_name, version);
    return 1;
}

int install_package(const char *package_name) {
    FILE *fp;
    char line[256];
    char name[256], version[64];
    int found = 0;
    fp = fopen(CPM_FILE, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", CPM_FILE);
        return 0;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%255s %63s", name, version) == 2) {
            if (package_name == NULL || strcmp(name, package_name) == 0) {
                found = 1;
                download_package(name, version);
                if (package_name != NULL) {
                    break;
                }
            }
        }
    }
    
    fclose(fp);
    if (package_name != NULL && !found) {
        fprintf(stderr, "Package %s not found in %s\n", package_name, CPM_FILE);
        return 0;
    }
    
    return 1;
}

void scan_source_files(const char *dir, char **src_files, int *src_count, int scan_libs) {
    DIR *d;
    struct dirent *entry;
    struct stat file_stat;
    char path[PATH_MAX];
    d = opendir(dir);
    if (!d) {
        return;
    }
    
    while ((entry = readdir(d)) != NULL) {
        snprintf(path, PATH_MAX, "%s/%s", dir, entry->d_name);
        if (stat(path, &file_stat) < 0) {
            continue;
        }
        
        if (S_ISREG(file_stat.st_mode)) {
            size_t len = strlen(entry->d_name);
            if (len > 2 && strcmp(entry->d_name + len - 2, ".c") == 0) {
                src_files[*src_count] = malloc(strlen(path) + 1);
                strcpy(src_files[*src_count], path);
                (*src_count)++;
            }
        } else if (S_ISDIR(file_stat.st_mode) && 
                  strcmp(entry->d_name, ".") != 0 && 
                  strcmp(entry->d_name, "..") != 0 &&
                  (scan_libs || strcmp(entry->d_name, CPM_DIR) != 0) &&
                  strcmp(entry->d_name, ".git") != 0) {
            scan_source_files(path, src_files, src_count, scan_libs);
        }
    }
    
    closedir(d);
}

void generate_makefile() {
    FILE *fp;
    char **src_files;
    char **lib_src_files;
    int src_count = 0;
    int lib_src_count = 0;
    int max_files = 1000;
    DIR *d;
    struct dirent *entry;
    struct stat file_stat;
    char path[PATH_MAX];
    src_files = (char **)malloc(max_files * sizeof(char *));
    lib_src_files = (char **)malloc(max_files * sizeof(char *));
    scan_source_files(".", src_files, &src_count, 0);
    fp = fopen(MAKEFILE, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create %s\n", MAKEFILE);
        for (int i = 0; i < src_count; i++) {
            free(src_files[i]);
        }
        free(src_files);
        free(lib_src_files);
        return;
    }
    
    fprintf(fp, "CC = gcc\n");
    fprintf(fp, "CFLAGS = -Wall -Wextra -std=c99 -I. -I%s/include", CPM_DIR);
    d = opendir(CPM_DIR);
    if (d) {
        while ((entry = readdir(d)) != NULL) {
            snprintf(path, PATH_MAX, "%s/%s", CPM_DIR, entry->d_name);
            if (stat(path, &file_stat) == 0 && 
                S_ISDIR(file_stat.st_mode) && 
                strcmp(entry->d_name, ".") != 0 && 
                strcmp(entry->d_name, "..") != 0) {
                fprintf(fp, " -I%s/%s/include -I%s/%s", CPM_DIR, entry->d_name, CPM_DIR, entry->d_name);
                char lib_dir[PATH_MAX];
                snprintf(lib_dir, PATH_MAX, "%s/%s", CPM_DIR, entry->d_name);
                scan_source_files(lib_dir, lib_src_files, &lib_src_count, 1);
            }
        }
        closedir(d);
    }
    
    fprintf(fp, "\n");
    fprintf(fp, "LDFLAGS = ");
    d = opendir(CPM_DIR);
    if (d) {
        while ((entry = readdir(d)) != NULL) {
            snprintf(path, PATH_MAX, "%s/%s", CPM_DIR, entry->d_name);
            if (stat(path, &file_stat) == 0 && 
                S_ISDIR(file_stat.st_mode) && 
                strcmp(entry->d_name, ".") != 0 && 
                strcmp(entry->d_name, "..") != 0) {
                fprintf(fp, " -L%s/%s/lib", CPM_DIR, entry->d_name);
            }
        }
        closedir(d);
    }
    
    fprintf(fp, "\n");
    fprintf(fp, "LDLIBS = ");
    d = opendir(CPM_DIR);
    if (d) {
        while ((entry = readdir(d)) != NULL) {
            snprintf(path, PATH_MAX, "%s/%s", CPM_DIR, entry->d_name);
            if (stat(path, &file_stat) == 0 && 
                S_ISDIR(file_stat.st_mode) && 
                strcmp(entry->d_name, ".") != 0 && 
                strcmp(entry->d_name, "..") != 0 &&
                strcmp(entry->d_name, "name:") != 0 &&
                strcmp(entry->d_name, "version:") != 0) {
                fprintf(fp, "-l%s ", entry->d_name);
            }
        }
        closedir(d);
    }
    
    fprintf(fp, "\n\n");
    char proj_name[256] = "program";
    FILE *cpmfp = fopen(CPM_FILE, "r");
    if (cpmfp) {
        char line[256];
        if (fgets(line, sizeof(line), cpmfp) && strncmp(line, "name:", 5) == 0) {
            sscanf(line + 5, "%255s", proj_name);
        }
        fclose(cpmfp);
    }
    
    fprintf(fp, "SRC = ");
    for (int i = 0; i < src_count; i++) {
        fprintf(fp, "%s ", src_files[i]);
    }
    fprintf(fp, "\n");
    fprintf(fp, "LIB_SRC = ");
    for (int i = 0; i < lib_src_count; i++) {
        fprintf(fp, "%s ", lib_src_files[i]);
    }
    fprintf(fp, "\n");
    fprintf(fp, "OBJ = $(SRC:.c=.o)\n");
    fprintf(fp, "LIB_OBJ = $(LIB_SRC:.c=.o)\n");
    fprintf(fp, "TARGET = %s\n\n", proj_name);
    fprintf(fp, "all: libs $(TARGET)\n\n");
    fprintf(fp, "libs: $(LIB_OBJ)\n\n");
    fprintf(fp, "$(TARGET): $(OBJ)\n");
    fprintf(fp, "\t$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)\n\n");
    fprintf(fp, "%%.o: %%.c\n");
    fprintf(fp, "\t$(CC) $(CFLAGS) -c $< -o $@\n\n");
    fprintf(fp, "clean:\n");
    fprintf(fp, "\trm -f $(OBJ) $(LIB_OBJ) $(TARGET)\n\n");
    fprintf(fp, ".PHONY: clean all libs\n");
    fclose(fp);
    for (int i = 0; i < src_count; i++) {
        free(src_files[i]);
    }
    for (int i = 0; i < lib_src_count; i++) {
        free(lib_src_files[i]);
    }
    free(src_files);
    free(lib_src_files);
    
    printf("Generated %s\n", MAKEFILE);
}

void show_help() {
    printf("cpm - C Package Manager v%s\n", CPM_VERSION);
    printf("Usage:\n");
    printf("  cpm init [project-name]    Initialize a new project\n");
    printf("  cpm install                Install all packages in %s\n", CPM_FILE);
    printf("  cpm install <package>      Install a specific package\n");
    printf("  cpm add <package> <ver>    Add a package to %s\n", CPM_FILE);
    printf("  cpm remove <package>       Remove a package\n");
    printf("  cpm update                 Update all packages\n");
    printf("  cpm list                   List installed packages\n");
    printf("  cpm makefile               Generate %s\n", MAKEFILE);
    printf("  cpm help                   Show this help\n");
}

void init_project(const char *project_name) {
    FILE *fp;
    char name[256] = "myproject";
    if (project_name != NULL) {
        strncpy(name, project_name, sizeof(name) - 1);
    }
    
    if (!ensure_directory_exists(CPM_DIR)) {
        return;
    }
    
    ensure_gitignore_updated();
    fp = fopen(CPM_FILE, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create %s\n", CPM_FILE);
        return;
    }
    
    fprintf(fp, "name: %s\n", name);
    fprintf(fp, "version: 0.1.0\n\n");
    fclose(fp);
    generate_makefile();
    printf("Initialized C project: %s\n", name);
}

void add_package(const char *package_name, const char *version) {
    FILE *fp;
    char line[256];
    char name[256], ver[64];
    int found = 0;
    char temp_file[] = "cpmfile.tmp";
    fp = fopen(CPM_FILE, "r");
    if (!fp) {
        fp = fopen(CPM_FILE, "w");
        if (!fp) {
            fprintf(stderr, "Failed to create %s\n", CPM_FILE);
            return;
        }
        fprintf(fp, "name: myproject\n");
        fprintf(fp, "version: 0.1.0\n\n");
        fprintf(fp, "%s %s\n", package_name, version);
        fclose(fp);
        printf("Added package %s version %s\n", package_name, version);
        return;
    }
    
    FILE *temp = fopen(temp_file, "w");
    if (!temp) {
        fprintf(stderr, "Failed to create temporary file\n");
        fclose(fp);
        return;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%255s %63s", name, ver) == 2 && strcmp(name, package_name) == 0) {
            fprintf(temp, "%s %s\n", package_name, version);
            found = 1;
        } else {
            fputs(line, temp);
        }
    }
    
    if (!found) {
        fprintf(temp, "%s %s\n", package_name, version);
    }
    
    fclose(fp);
    fclose(temp);
    if (rename(temp_file, CPM_FILE) != 0) {
        fprintf(stderr, "Failed to update %s\n", CPM_FILE);
        return;
    }
    
    printf("%s package %s version %s\n", found ? "Updated" : "Added", package_name, version);
}

void remove_package(const char *package_name) {
    FILE *fp;
    char line[256];
    char name[256], version[64];
    int found = 0;
    char temp_file[] = "cpmfile.tmp";
    fp = fopen(CPM_FILE, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", CPM_FILE);
        return;
    }
    
    FILE *temp = fopen(temp_file, "w");
    if (!temp) {
        fprintf(stderr, "Failed to create temporary file\n");
        fclose(fp);
        return;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%255s %63s", name, version) == 2 && strcmp(name, package_name) == 0) {
            found = 1;
        } else {
            fputs(line, temp);
        }
    }
    
    fclose(fp);
    fclose(temp);
    if (!found) {
        fprintf(stderr, "Package %s not found in %s\n", package_name, CPM_FILE);
        remove(temp_file);
        return;
    }
    
    if (rename(temp_file, CPM_FILE) != 0) {
        fprintf(stderr, "Failed to update %s\n", CPM_FILE);
        return;
    }
    
    char package_dir[512];
    snprintf(package_dir, sizeof(package_dir), "%s/%s", CPM_DIR, package_name);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", package_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to remove package directory %s\n", package_dir);
    }
    
    printf("Removed package %s\n", package_name);
}

void list_packages() {
    FILE *fp;
    char line[256];
    char name[256], version[64];
    int count = 0;
    fp = fopen(CPM_FILE, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open %s\n", CPM_FILE);
        return;
    }
    
    printf("Installed packages:\n");
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%255s %63s", name, version) == 2) {
            printf("  %s - %s\n", name, version);
            count++;
        }
    }
    
    fclose(fp);
    if (count == 0) {
        printf("  No packages installed\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        show_help();
        return 1;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    if (strcmp(argv[1], "init") == 0) {
        init_project(argc > 2 ? argv[2] : NULL);
    } else if (strcmp(argv[1], "install") == 0) {
        if (!ensure_directory_exists(CPM_DIR)) {
            return 1;
        }
        ensure_gitignore_updated();
        if (argc > 2) {
            install_package(argv[2]);
        } else {
            install_package(NULL);
        }
        
        generate_makefile();
    } else if (strcmp(argv[1], "add") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: cpm add <package> <version>\n");
            return 1;
        }
        add_package(argv[2], argv[3]);
    } else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cpm remove <package>\n");
            return 1;
        }
        remove_package(argv[2]);
        generate_makefile();
    } else if (strcmp(argv[1], "list") == 0) {
        list_packages();
    } else if (strcmp(argv[1], "makefile") == 0) {
        generate_makefile();
    } else if (strcmp(argv[1], "help") == 0) {
        show_help();
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        show_help();
        return 1;
    }
    
    curl_global_cleanup();
    return 0;
}
