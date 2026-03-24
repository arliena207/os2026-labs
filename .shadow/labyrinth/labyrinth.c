#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <testkit.h>
#include "labyrinth.h"

int main(int argc, char *argv[]) {
    // TODO: Implement this function

    char *mapfilename = NULL;
    char playerId = '\0';
    char *direction = NULL;
    bool is_move = false;
    bool showversion = false;

    // 解析命令行参数
    for (int i = 1 ; i<argc ;i++){
        if (strcmp(argv[i],"--map")==0 || strcmp(argv[i],"-m")==0){
            if (i +1 >= argc){
                return 1;
            }
            mapfilename=argv [++i];
        }else if (strcmp(argv[i],"--player")==0 || strcmp(argv[i],"-p")==0){
            if (i +1 >= argc){
                return 1;
            }
            playerId=argv [++i][0];
        }else if (strcmp(argv[i],"--move")==0 ){
            if (i +1 >= argc){
                return 1;
            }
            direction=argv [++i];
            is_move=true;
        }else if (strcmp(argv[i],"--version")==0 ){
            showversion=true;
        }else{
            return 1;
        }
    }

    // version 
    if (showversion){
        if (argc != 2){
            return 1;
        }
        printf("Labyrinth Game\n");
        return 0;
    }

    // 参数合法性检查
    if (mapfilename == NULL || !isValidPlayer(playerId)){
        printUsage();
        return 1;
    }
    Labyrinth labyrinth;

    if(!loadMap(&labyrinth,mapfilename)){
        return 1;
    }

    if (!isConnected(&labyrinth)){
        return 1;
    }

    if (!is_move){
        for (int i = 0;i<labyrinth.rows;i++){
            for (int j = 0;j<labyrinth.rows;j++){
            putchar(labyrinth.map[i][j]);
        }
        putchar('\n');
    }
    return 0;
    }

    if(!movePlayer(&labyrinth,playerId,direction)){
        return 1;
    }

    if (!saveMap(&labyrinth,mapfilename)){
        return 1;
    }

    // 流程控制
    return 0;
}

void printUsage() {
    printf("Usage:\n");
    printf("  labyrinth --map map.txt --player id\n");
    printf("  labyrinth -m map.txt -p id\n");
    printf("  labyrinth --map map.txt --player id --move direction\n");
    printf("  labyrinth --version\n");
}

bool isValidPlayer(char playerId) {
    // TODO: Implement this function
    int n = playerId - '0';
    if (n >= 0 && n <= 9){
        return true;
    } else {
    return false;
    }
}

bool loadMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    if (filename == NULL){
        return false;
    }

    FILE *f = fopen(filename,"r");
    if (f == NULL){
        return false;
    }

    char line[MAX_COLS+2];
    int rows = 0;
    int cols = -1;

    while(fgets(line,sizeof(line),f) != NULL){
        size_t len = strlen(line);

        if (len > 0 && line[len-1]=='\n'){
            line[len-1]='\0';
            len--;
        }

        if (len == 0){
            fclose(f);
            return false;
        }

        if ((int)len>MAX_COLS || rows >= MAX_ROWS){
            fclose(f);
            return false;
        }

        if (cols == -1){
            cols = (int) len;
        }else if ((int)len != cols){
            fclose(f);
            return false;
        }

        for (int c = 0; c<cols;c++){
            char ch = line[c];
            if (!(ch == '#'||ch=='.'||isValidPlayer(ch))){
                fclose(f);
                return false;
            }
            labyrinth->map[rows][c]=ch;
        }
        rows ++;
    }

    fclose(f);

    if (rows == 0 || cols <= 0){
        return false;
    }

    labyrinth->cols=cols;
    labyrinth->rows=rows;
    return true;
}

Position findPlayer(Labyrinth *labyrinth, char playerId) {
    // TODO: Implement this function
    Position pos = {-1, -1};

    if (labyrinth == NULL){
        return pos;
    }

    for (int i = 0;i <labyrinth->rows;i++){
        for(int j = 0;j<labyrinth->cols;j++){
            if (labyrinth->map[i][j] == playerId){
                pos.row= i;
                pos.col = j;
                return pos;
            }
        }
    }
    return pos;
}

Position findFirstEmptySpace(Labyrinth *labyrinth) {
    // TODO: Implement this function
    Position pos = {-1, -1};

    if (labyrinth == NULL){
        return pos;
    }

    for (int i = 0;i <labyrinth->rows;i++){
        for(int j = 0;j<labyrinth->cols;j++){
            if (labyrinth->map[i][j] == '.'){
                pos.row = i;
                pos.col = j;
                return pos;
            }
        }
    }
    return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
    // TODO: Implement this function
    if (labyrinth == NULL){
        return false;
    }

    if (row < 0 || row >= labyrinth->rows || col <0 ||col >= labyrinth->cols){
        return false;
    }
    return labyrinth->map[row][col] == '.';
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
    // TODO: Implement this function
    if (labyrinth == NULL || direction == NULL || !isValidPlayer(playerId)){
        return false;
    }

    int row = 0,col = 0;
    if (strcmp(direction,"up")==0){
        row = -1;
    }else if (strcmp(direction,"down")==0){
        row = 1;
    }else if (strcmp(direction,"left")==0){
        col = -1;
    }else if (strcmp(direction,"right")==0){
        col = 1;
    }

    Position curr = findPlayer(labyrinth,playerId);
    bool player_exist = true;

    if (curr.row == -1 || curr.col == -1){
        curr = findFirstEmptySpace(labyrinth);
        player_exist=false;
        if (curr.row == -1 || curr.col == -1){
            return false;
        }
    }

    int nextrow = curr.row + row;
    int nextcol = curr.col + col;

    if (!isEmptySpace(labyrinth,nextrow,nextcol)){
        return false;
    }

    if (player_exist){
        labyrinth->map[curr.row][curr.col] = '.';
    }

    labyrinth->map[nextrow][nextcol]=playerId;
    return true;

}

bool saveMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    if (labyrinth == NULL || filename == NULL){
        return false;
    }

    FILE *f = fopen(filename,"w");
    if (f == NULL){
        return false;
    }

    for (int i = 0 ; i <labyrinth->rows;i++){
        for (int j = 0 ;j <labyrinth->cols;j++){
            fputc(labyrinth->map[i][j],f);
        }
        fputc('\n',f);
    }

    fclose(f);
    return true;
}

// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col, bool visited[MAX_ROWS][MAX_COLS]) {
    // TODO: Implement this function
    if (labyrinth == NULL){
        return ;
    }

    if (row < 0 || row >= labyrinth->rows || col <0 ||col >= labyrinth->cols){
        return ;
    }

    if (visited[row][col]){
        return ;
    }

    char ch = labyrinth->map[row][col];

    if (!(ch == '.' || isValidPlayer(ch))){
        return ;
    }

    visited[row][col]=true;

    dfs(labyrinth,row-1,col,visited);
    dfs(labyrinth,row+1,col,visited);
    dfs(labyrinth,row,col-1,visited);
    dfs(labyrinth,row,col+1,visited);
}

bool isConnected(Labyrinth *labyrinth) {
    // TODO: Implement this function
    if (labyrinth == NULL){
        return false;
    }

    bool visited[MAX_ROWS][MAX_COLS]={{false}};
    Position start = {-1,-1};

    for (int i = 0;i <labyrinth->rows;i++){
        for (int j = 0;j<labyrinth->cols;j++){
            char ch = labyrinth->map[i][j];
            if (ch == '.' || isValidPlayer(ch)){
                start.row=i;
                start.col=j;
                break;
            }
        }
        if (start.row != -1){
        break;
        }
    }

    if (start.row == -1){
        return true;
    }

    dfs(labyrinth,start.row,start.col,visited);

    for (int i = 0;i <labyrinth->rows;i++){
        for (int j = 0;j<labyrinth->cols;j++){
            char ch = labyrinth->map[i][j];
            if ((ch == '.' || isValidPlayer(ch)) && !visited[i][j]){
                return false;
            }
        }
    }

    return true;
}
