#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>

//STRUCTS
typedef struct pgt_second{
unsigned int paddr;
int valid;
}pgt_second;

typedef struct pgt_entry {
pgt_second *pointer;
int valid;
}pgt_entry;

typedef struct cache_block {
int valid;
int paddr;
int value;
}cache_block;

typedef struct thread{
pthread_t tid;
unsigned long tid2;
unsigned int vaddr[64];
int in_use_vaddr[64];
pgt_entry pgt_dir[1024];
int kill;
}thread;

thread threads[4];
cache_block cache[4];
//   FUNCTIONS


int thread_add(pthread_t tid){
    int i=0;
    for(i=0;i<4;i++){
        if(threads[i].tid2==0){
            threads[i].tid=tid;
            threads[i].tid2=(unsigned long)tid;
            threads[i].pgt_dir[0].pointer = (pgt_second*)calloc(1024,sizeof(pgt_second));
            threads[i].pgt_dir[0].valid = 1;
            return 0;
        }
    }
    return 1;
}

int thread_location(unsigned long tid){
    int i=0;
    for(i=0;i<4;i++){
        if(threads[i].tid2==tid){
            return i;
        }
    }
    return -1;
}

void* threadfunction(void *vargp){
    while(1){
        if(threads[thread_location((unsigned long)pthread_self())].kill==1){
            break;
        }
    }
    return NULL;
}

void add_virtualaddr(int threadindex, unsigned int n){
    int i=0;
    for(i=0;i<64;i++){
        if(threads[threadindex].in_use_vaddr[i]==0){
           threads[threadindex].vaddr[i] = n;
           threads[threadindex].in_use_vaddr[i]=1;
           break; 
        }
    }
}
int checkSpace_Vaddr(int threadindex){
    int i=0;
    for(i=0;i<64;i++){
        if(threads[threadindex].in_use_vaddr[i]==0){
           return 0;
        }
    }
    return -1;
}
void binprintf(unsigned int v)
{
    int counter=1;
    unsigned int mask=1<<((sizeof(int)<<3)-1);
    while(mask) {
        printf("%d", (v&mask ? 1 : 0));
        mask >>= 1;
        if(counter<30){
            if(counter % 10 == 0)
                printf("  ");
            counter++;    
        }
    }
    printf("\n");
}

unsigned int cse320_malloc(int threadindex,int number){
    int i;
    int pagei=0;
REPEAT:
    if(pagei>0 && threads[threadindex].pgt_dir[pagei].valid == 0){
            threads[threadindex].pgt_dir[pagei].pointer = (pgt_second*)calloc(1024,sizeof(pgt_second));
            threads[threadindex].pgt_dir[pagei].valid = 1;
    }    
    for(i=0;i<1024;i++){
        if(threads[threadindex].pgt_dir[pagei].pointer[i].valid == 0){
            threads[threadindex].pgt_dir[pagei].pointer[i].valid = 1;
            threads[threadindex].pgt_dir[pagei].pointer[i].paddr = number;
            unsigned int virt = 0;
            virt = (virt | pagei) << 10;
            virt = (virt | i);
            virt = virt << 12;
            add_virtualaddr(threadindex,virt); 
            return virt;
        }
    }
    pagei++;
    if(pagei>1023){
        printf("all the page tables are full");
        return -1;
        
    }
    goto REPEAT;
}

int cse320_virt_to_phys(int location , unsigned int vaddr){
    
    unsigned int first_pgt= vaddr>>22;
    unsigned int second_pgt =(vaddr<<10)>>22;
    unsigned int last_bits = (vaddr<<20)>>20;
    if(first_pgt>1023 || second_pgt>1023 || last_bits!=0){
        printf("invalid virtual address\n");
        return -1;
    }
    if(threads[location].pgt_dir[first_pgt].valid == 0)
    {
        printf("address not found \n");
        return -1;
    }
    else{
        if(threads[location].pgt_dir[first_pgt].pointer[second_pgt].valid == 0){
            printf("address not found \n");
            return -1;
        } 
        else{
            int addr = threads[location].pgt_dir[first_pgt].pointer[second_pgt].paddr;
            return addr;
        }
    }
    return 0;
}


int cache_exists(int phys_addr){
    int i=0;
    for(i=0;i<4;i++){
        if(cache[i].valid==1 && cache[i].paddr==phys_addr){
            printf("cache hit\n");
            return cache[i].value;
        }
    }
    printf("cache miss\n");
    return -1;
}
int cache_add(int paddr,int value){ 
    int i=0;
    for(i=0;i<4;i++){
        if(cache[i].valid==0 || cache[i].paddr==paddr){
            cache[i].valid=1;
            cache[i].paddr=paddr;
            cache[i].value=value;
            return 0;
        }
    }
    int Rnumber = rand() % 4;
    cache[Rnumber].valid=1;
    cache[Rnumber].paddr=paddr;
    cache[Rnumber].value=value;
    printf("eviction \n");
    return 1;
}

void pgt_free(int index){
    int i;
    for(i =0;i<1024;i++){
        if(threads[index].pgt_dir[i].valid==1){
            free(threads[index].pgt_dir[i].pointer);
        }
    }
}
//   MAIN()

int main(int argc,char **argv) 
{
    int loop=1;
    int i=0;
    int status;
    sigset_t mask_child;
    sigemptyset(&mask_child);
    sigaddset(&mask_child,SIGCHLD);
    char buf[100]; //input buffer
    printf("type 'help' for help\n");
    int fd;
    mkfifo("fifo", 0666);
    while(loop==1){
LOOP:
        printf("Menu: \n \n");
        printf("create\n");
        printf("kill X\n");
        printf("list\n");
        printf("mem X\n");
        printf("allocate X\n");
        printf("read X Y\n");
        printf("write X Y Z\n");
        printf("quit\n \n");
        printf("Enter your choice: ");
        fflush(stdin);
        scanf(" %[^\n][^EOF]%*c",buf);
        int i = 0;
        int choice;
        char *p = strtok (buf, " ");
        char *array[7]={NULL};
        int counter = -1;
        while (p != NULL)
        {
            array[i++] = p;
            p = strtok (NULL, " ");
            counter++;
        }
            if(strcmp(array[0],"create")==0) 
            {
                if(counter>0){
                    printf("too many inputs. \n");
                    goto LOOP;
                }
                    pthread_t tid;
                    pthread_create(&tid,NULL,threadfunction,NULL);
                    int ret  = thread_add(tid);
                    if(ret == 1){
                        printf("MAX amount of threads reached. \n");
                        pthread_cancel(tid);
                    }
            }
            else if(strcmp(array[0],"kill")==0) 
            {
                if(counter>1){
                    printf("too many inputs. \n");
                    goto LOOP;
                }
                int location = thread_location(strtoul(array[1],NULL,10));
                if(location==-1){
                    printf("Thread ID does not exist \n");
                    goto LOOP;
                }
                int i;  
                for(i=0;i<64;i++){
                    if(threads[location].in_use_vaddr[i]==1){
                        int result = cse320_virt_to_phys(location,threads[location].vaddr[i]);
                        fd = open("fifo",O_WRONLY);
                        char str[100];
                        char s[10];
                        sprintf(s,"%d", result);
                        strcpy(str,"kill ");
                        strcat(str,s);
                        write(fd,str,strlen(str)+1);
                        close(fd);
                        fd = open("fifo",O_RDONLY);
                        int ret;
                        read(fd,&ret,sizeof(int));
                        close(fd);
                    }
                }
                pgt_free(location); 
                threads[location].kill=1;
                pthread_join(threads[location].tid,NULL);
                threads[location]=(struct thread){ 0 }; 
            }
            else if(strcmp(array[0],"list")==0){
                for(i=0;i<4;i++){
                    if(threads[i].tid2!=0){
                        printf("%lu \n",threads[i].tid2);
                    }
                }
            }
            else if(strcmp(array[0],"mem")==0){
                int location = thread_location(strtoul(array[1],NULL,10));
                if(location==-1){
                    printf("Thread ID does not exist \n");
                    goto LOOP;
                }
                int i;
                for(i=0;i<64;i++){
                    if(threads[location].in_use_vaddr[i]!=0)
                        binprintf(threads[location].vaddr[i]);
                }
            }
            else if(strcmp(array[0],"allocate")==0){
                if(counter>1){
                    printf("too many inputs. \n");
                    goto LOOP;
                }
                int location = thread_location(strtoul(array[1],NULL,10));
                if(location > -1){
                int ret = checkSpace_Vaddr(location);
                if(ret==-1){
                    printf("No memory for Virtual Address. . \n");
                    goto LOOP;
                }
                    fd = open("fifo",O_WRONLY);
                    char str[80];
                    strcpy(str,"allocate");
                    write(fd,str,strlen(str)+1);
                    close(fd);
                    fd = open("fifo",O_RDONLY);
                    int index=0;
                    read(fd,&index,sizeof(int));
                    if(index == -1){
                        printf("physical address is full \n");
                        close(fd);
                        goto LOOP;
                    }
                    cse320_malloc(location,index);
                    close(fd);
                }
                else{
                    printf("Thread ID does not exist \n");
                    goto LOOP;
                } 
            }
            else if(strcmp(array[0],"read")==0){
                if(counter<2 || counter>2){
                    printf("invalid number of inputs. \n");
                    goto LOOP;
                }
                int location = thread_location(strtoul(array[1],NULL,10));
                if(location>-1){
                    int result = cse320_virt_to_phys(location,strtoul(array[2],NULL,2));
                    if(result>-1){       
                        int n;
                        if((n = cache_exists(result))!=-1){
                            printf("Physical address value from cache : %d \n",n);
                            goto LOOP;
                        }
                        fd = open("fifo", O_WRONLY);
                        if(fd<1) {
                            printf("Error opening file\n");
                            goto LOOP;
                        }
                        char str[80];
                        char s[11];
                        sprintf(s,"%d", result);
                        strcpy(str,"read ");
                        strcat(str,s);
                        write(fd,str,strlen(str)+1);
                        close(fd);
                        fd = open("fifo",O_RDONLY);
                        if(fd<1) {
                                printf("Error opening file\n");
                                goto LOOP;
                        }
                        int value;
                        read(fd,&value,sizeof(int));
                        if(value!=-1){
                            printf("Physical address value : %d \n",value);
                            cache_add(result,value);
                        }
                        close(fd);
                    }
                }
                else{
                    printf("Thread ID does not exist \n");
                    goto LOOP;
                }
            }
            else if(strcmp(array[0],"write")==0){
                if(counter<3 || counter>3){
                    printf("invalid number of inputs. \n");
                    goto LOOP;
                }
                int location = thread_location(strtoul(array[1],NULL,10));
                if(location>-1){
                    int result = cse320_virt_to_phys(location,strtoul(array[2],NULL,2));
                    if(result>-1){          
                        fd = open("fifo", O_WRONLY);
                            if(fd<1) {
                                 printf("Error opening file\n");
                                 goto LOOP;
                            }
                            char str[80];
                            char s[11];
                            sprintf(s,"%d ", result);
                            strcpy(str,"write ");
                            strcat(str,s);
                            strcat(str,array[3]);
                            write(fd,str,strlen(str)+1);
                            close(fd);
                            
                            fd = open("fifo",O_RDONLY);
                            if(fd<1) {
                                printf("Error opening file");
                                close(fd);
                                goto LOOP;
                            }
                            char str3[255];
                            read(fd,str3,255);
                            printf("%s\n",str3);
                            close(fd);
                            cache_add(result,atoi(array[3]));
                        }
                    }
                else{
                    printf("Thread ID does not exist \n");
                    goto LOOP;
                }
            }
            else if(strcmp(array[0],"quit")==0){
                int fd = open("fifo", O_WRONLY);
                if(fd<1) {
                    printf("Error opening file");
                    goto LOOP;
                }
                int i;
                for(i=0;i<4;i++){
                    if(threads[i].tid2!=0){
                        threads[i].kill=1;
                        pthread_join(threads[i].tid,NULL);
                        pgt_free(i);
                    }
                }
                char str[80];
                strcpy(str,"quit");
                write(fd,str,strlen(str)+1);
                close(fd);
                break;
            }
            else{
                printf("invalid command. Please try again.\n");
            }
    }    
    return 0;
}

