#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

static void print_verison(void){
    printf("pstree 1.0\n");
}

static void print_usage(const char *prog){
    fprintf(stderr,"Usage:%s [-p]--show-pids [-n]--numeric-sort [-V]--version\n",prog);
}


static int read_comm(pid_t pid, char *buf, size_t n) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, (int)n, f)) { fclose(f); return -1; }
    buf[strcspn(buf, "\n")] = 0;
    fclose(f);
    return 0;
}

static int get_ppid_from_stat(pid_t pid, pid_t *ppid_out) {
    char path[64], line[4096];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
    fclose(f);

    int id, ppid;
    char comm[256], state;
    if (sscanf(line, "%d (%255[^)]) %c %d", &id, comm, &state, &ppid) != 4) return -1;
    *ppid_out = (pid_t)ppid;
    return 0;
}

int main(int argc, char *argv[]) {

    // 开关
    int show_pids=0;
    int version=0;
    int numeric_sort=0;

    // 参数解析
    for (int i = 1;i<argc;i++){
        if (strcmp(argv[i],"-p")==0 || strcmp(argv[i],"--show-pids")==0){
            show_pids=1;
        }
        else if (strcmp(argv[i],"-n")==0 || strcmp(argv[i],"--numeric-sort")==0){
            numeric_sort=1;
        }
        else if (strcmp(argv[i],"-V")==0 || strcmp(argv[i],"--version")){
            version=1;
        } 
        else{
            fprintf(stderr,"Invalid option : %s\n",argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (version){
        print_verison();
        return 0;
    }

    pid_t self = getpid();
    pid_t parent = getppid();

    char self_comm[256] = "?", parent_comm[256] = "?";
    read_comm(self, self_comm, sizeof self_comm);
    read_comm(parent, parent_comm, sizeof parent_comm);

    printf("%s(%d)\n", parent_comm, parent);

    DIR *d = opendir("/proc");
    if (!d) { perror("opendir /proc"); return 1; }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;
        pid_t pid = (pid_t)atoi(de->d_name);

        pid_t ppid;
        if (get_ppid_from_stat(pid, &ppid) != 0) continue;
        if (ppid != parent) continue;

        char comm[256] = "?";
        read_comm(pid, comm, sizeof comm);

        printf("  |- %s(%d)%s\n", comm, pid, (pid == self) ? "  <== me" : "");
    }

    closedir(d);
    return 0;
}
