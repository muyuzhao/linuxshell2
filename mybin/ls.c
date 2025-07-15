#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
int main(){

	char path[256]={0};
	if(getcwd(path,256)==NULL){
		perror("getcwd err!");
		exit(1);
	}
	DIR *pdir=opendir(path);
	if(pdir==NULL){
		perror("opendir err!");
		exit(0);
	}
	struct dirent *s=NULL;
	while((s=readdir(pdir))!=NULL){
		if(strncmp(s->d_name,".",1)==0){
			continue;
		}
		struct stat filestat;
		stat(s->d_name,&filestat);
		if(S_ISDIR(filestat.st_mode)){
			printf("\033[1;34m%s\033[0m   ",s->d_name);
		}else{
			if(filestat.st_mode &(S_IXUSR|S_IXGRP|S_IXOTH)){
				printf("\033[1;32m%s\033[0m   ",s->d_name);
			}else{
				printf("%s ",s->d_name);
			}
		}
	}
	printf("\n");

	closedir(pdir);
	exit(0);
}

