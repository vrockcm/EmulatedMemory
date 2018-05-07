#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

void* emulatedmem;
int in_use[256];

int find_block(){
    int i;
    for(i=0;i<256;i++){
        if(in_use[i]==0){
            return i;
        }
    }
    return -1;
}

int main(int argc,char **argv) 
{ 
    sleep(5);
    emulatedmem = calloc(1024,1);
    char buf[100]; 
    mkfifo("fifo", 0666);
    while(1){
        int fd1=open("fifo",O_RDONLY);
        int choice; 
        read(fd1,buf,100);
        close(fd1);
        int i = 0;
        char *p = strtok (buf, " ");
        char *array[7]={NULL};
        while (p != NULL)
        {
            array[i++] = p;
            p = strtok (NULL, " ");
        }
        if(strcmp(array[0],"read")==0) 
        {
            fd1 = open("fifo",O_WRONLY);
            int addr = atoi(array[1]);
            if(addr<0 || addr>1023){
                printf("error, address out of range\n");
                int ret=-1;
                write(fd1,&ret, sizeof(int));
                close(fd1);
            }
            else if(addr%4!=0){
                printf("error, address is not aligned\n");
                int ret=-1;
                write(fd1,&ret, sizeof(int));
                close(fd1);
            }
            else{
                if(in_use[(addr/4)]==1){
                    write(fd1,&((int*)emulatedmem)[addr],sizeof(int));
                    close(fd1);
                }
                else{ 
                    printf("error, address not in use\n");
                    int ret=-1;
                    write(fd1,&ret, sizeof(int));
                    close(fd1);
                }
            }    
        }
        else if(strcmp(array[0],"write")==0) 
        {
            fd1 = open("fifo",O_WRONLY);
            int addr = atoi(array[1]);
            if(addr<0 || addr>1023){
                char* str2="error, address out of range\n";
                write(fd1,str2, strlen(str2)+1);
                close(fd1);
            }
            else if(addr%4!=0){
                char* str2="error, address is not aligned\n";
                write(fd1,str2, strlen(str2)+1);
                close(fd1);
            }
            else{
                int value = atoi(array[2]);
                ((int*)emulatedmem)[addr]=value;
                in_use[(addr/4)]=1;
                char* str2="Success! \n"; 
                write(fd1,str2, strlen(str2)+1);
                close(fd1);
            }    
        }
        else if(strcmp(array[0],"kill")==0) 
        {
            int addr = atoi(array[1]);
            in_use[addr/4]=0; 
            fd1 = open("fifo",O_WRONLY);
            int ret = 0;
            write(fd1,&ret,sizeof(int));
            close(fd1);
                           
        }
        else if(strcmp(array[0],"allocate")==0) 
        {
                fd1 = open("fifo",O_WRONLY);
                int space = find_block();
                if(space!=-1){
                    in_use[space]=1;
                    space = space*4;
                    write(fd1,&space,sizeof(int));
                }
                else{
                    char str[10];
                    int ret = -1;
                    write(fd1,&ret, sizeof(int));
                }
                close(fd1);
        }
        else if(strcmp(array[0],"quit")==0){
            free(emulatedmem);
            break;
        }

    }
    return 0;
}
