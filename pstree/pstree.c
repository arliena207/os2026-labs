#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

// 进程节点
typedef struct Process{
    pid_t pid;
    pid_t ppid;
    char name[128];

    int *children;
    int child_count;
    int child_capacity;
}Process;

static void print_verison(void){
    printf("pstree 1.0\n");
}

static void print_usage(const char *prog){
    fprintf(stderr,"Usage:%s [-p]--show-pids [-n]--numeric-sort [-V]--version\n",prog);
}

static int add_root(int **roots,int *count,int *capacity,int idx){
    if(*count>=*capacity){
        int new_capacity = (*capacity == 0)? 8 : (*capacity*2);
        int *new_roots=realloc(*roots,new_capacity*sizeof(int));
        if(!new_roots){
            return -1;
        }
        *roots=new_roots;
        *capacity=new_capacity;
    }
    (*roots)[(*count)++]=idx;
    return 0;
}

static int add_process(Process **process,int *count,int *capacity,pid_t pid,pid_t ppid,const char *name){
    if(*count>=*capacity){
        int new_capacity = (*capacity == 0)?64 : (*capacity*2);
        Process *new_process=realloc(*process,new_capacity*sizeof(Process));
        if(!new_process){
            return -1;
        }
        *process=new_process;
        *capacity=new_capacity;
    }

    Process *p=&((*process)[*count]);
    p->pid=pid;
    p->ppid=ppid;
    strncpy(p->name,name,sizeof(p->name)-1);
    p->name[sizeof(p->name)-1]='\0';

    p->children=NULL;
    p->child_count=0;
    p->child_capacity=0;

    (*count)++;
    return 0;
}

static int find_process_index(Process *process,int count,pid_t pid){
    for (int i = 0;i<count;i++){
        if(process[i].pid==pid){
            return i;
        }
    }
    return -1;
}

static int add_child(Process *process,int child_index){
    if(process->child_count >= process->child_capacity){
        int new_capacity = (process->child_capacity == 0)?4:(process->child_capacity *2);
        int *new_child=realloc(process->children,new_capacity*sizeof(int));
        if(!new_child)return -1;
        process->child_capacity=new_capacity;
        process->children=new_child;
    }

    process->children[process->child_count++]=child_index;
    return 0;
}

static void print_tree(Process *procs,int idx,int depth,int show_pids){
    for (int i =0;i<depth;i++){
        printf("   ");
    }

    if(show_pids){
        printf("%s(%d)\n",procs[idx].name,procs[idx].pid);
    }else{
        printf("%s\n",procs[idx].name);
    }

    for(int i = 0;i<procs[idx].child_count;i++){
        int child_idx=procs[idx].children[i];
        print_tree(procs,child_idx,depth+1,show_pids);
    }
}

static void sort_children_by_pid(Process *procs,Process *parent){
    for (int i = 0;i<parent->child_count;i++){
        for (int j=0;j<parent->child_count;j++){
            int idx1=parent->children[i];
            int idx2=parent->children[j];

            if(procs[idx1].pid>procs[idx2].pid){
                int t=parent->children[i];
                parent->children[i]=parent->children[j];
                parent->children[j]=t;
            }
        }
    }
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
        else if (strcmp(argv[i],"-V")==0 || strcmp(argv[i],"--version")==0){
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

    Process *procs=NULL;
    int proc_count=0;
    int proc_capacity=0;
    int *roots=NULL;
    int root_count=0;
    int root_capacity=0;

    DIR *d = opendir("/proc");
    if (!d) { perror("opendir /proc"); return 1; }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)de->d_name[0])) continue;
        pid_t pid = (pid_t)atoi(de->d_name);

        pid_t ppid;
        if (get_ppid_from_stat(pid, &ppid) != 0) continue;

        char comm[256] = "?";
        if(read_comm(pid, comm, sizeof (comm))!=0){
            continue;
        }

        if (add_process(&procs,&proc_count,&proc_capacity,pid,ppid,comm)!=0){
            fprintf(stderr,"memory allocate failed!!!");
            closedir(d);
            free(procs);
            return 1;
        }
    }

    closedir(d);

    // 建树
    for (int i = 0;i<proc_count;i++){
        int parent_idx=find_process_index(procs,proc_count,procs[i].ppid);

        if(parent_idx>=0){
            if(add_child(&procs[parent_idx],i)!=0){
                fprintf(stderr,"memory allocate failed!!!\n");
                for (int j =0;j<proc_count;j++){
                    free(procs[j].children);
                }
                free(procs);
                free (roots);
                return 1;
            }
        }else{
            if(add_root(&roots,&root_count,&root_capacity,i)!=0){
                fprintf(stderr,"memory allocate failed!!!\n");
                for (int j =0;j<proc_count;j++){
                    free(procs[j].children);
                }
                free(procs);
                free (roots);
                return 1;
            }
        }
    }

    if (numeric_sort){
        for (int i =0;i<proc_count;i++){
            sort_children_by_pid(procs,&procs[i]);
        }
    }

    // print
    for(int i=0;i<root_count;i++){
        print_tree(procs,roots[i],0,show_pids);
    }

    // free memory
    for(int i=0;i<proc_count;i++){
        free(procs[i].children);
    }
    free(procs);
    free(roots);


    return 0;
}
