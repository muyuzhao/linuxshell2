#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 

int main(){
	char path[256]={0};
	if(getcwd(path,256)==NULL){
		perror("getcwd err!");
		exit(1);
	}
	printf("%s\n",path);
	exit(0);
}
