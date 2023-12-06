#include "fileManager.h"

#include <string.h>
#include <stdlib.h>
#include <dirent.h>

int list_jobs(){
    DIR *dirp;
    struct dirent *dp;
    dirp = opendir("./jobs");
    if (dirp == NULL) {
        errMsg("opendir failed");
        return;
    }
    
    while(dp = readdir(dirp) != NULL){
        dp = readdir(dirp);
    
        if (strstr(dp->d_name, ".job") != NULL){
            

        }
    }
    closedir(dirp);
}