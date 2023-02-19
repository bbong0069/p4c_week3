#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

char* HTML_400="<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>P4C Week3 HTTP Server</title></head><body><h1>400 Bad Request</h1></body></html>";
char* HTML_404="<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>P4C Week3 HTTP Server</title></head><body><h1>404 Not Found</h1></body></html>";
char* HTML_500="<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>P4C Week3 HTTP Server</title></head><body><h1>500 Internal Server Error</h1></body></html>";

void *handle_client(void* clnt_socket);
void make_header(char* header,char* state,char* cnt_type,long len);
void check_content_type(char* cnt_type,char* uri);

int main(int argc,char** argv) {
    int serv_sock,clnt_sock;
    struct sockaddr_in serv_addr,clnt_addr;
    int clnt_addr_size;
    pthread_t thread;

    if(argc!=2){
        printf("Input : %s <port>\n",argv[0]);
        exit(1); 
    }

    if((serv_sock=socket(PF_INET,SOCK_STREAM,0))==-1){
        perror("socket() error : ");
        exit(1);
    }
    else
        printf("socket() success\n");

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    serv_addr.sin_port=htons(atoi(argv[1]));

    if(bind(serv_sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr))==-1){
        perror("bind() error : ");
        exit(1);
    }
    else
        printf("bind() success\n");

    if(listen(serv_sock,20)==-1){
        perror("listen() error : ");
        exit(1);
    }
    else 
        printf("listen() success\n");

    while(1){
        clnt_addr_size=sizeof(clnt_addr);
        clnt_sock=accept(serv_sock,(struct sockaddr*)&clnt_addr,&clnt_addr_size);
        if(clnt_sock==-1){
            perror("accept() error : ");
            exit(1);
        }
        printf("connection request <ip> %s <port>%d\n",inet_ntoa(clnt_addr.sin_addr),ntohs(clnt_addr.sin_port));
        pthread_create(&thread,NULL,handle_client,&clnt_sock);    
    }
    
    return 0;
}
void* handle_client(void *sock){
    int clnt_sock=*(int*)sock;
    char rcv_msg[1024];
    char msg[2048];
    char* file_name;
    FILE* fp;
    long f_len;

    int state;

    char cnt_type[20];
    char header[1024];

    memset(rcv_msg,0,sizeof(rcv_msg));
    memset(msg,0,sizeof(msg));

    if(read(clnt_sock,rcv_msg,sizeof(rcv_msg)-1)==-1){
        perror("read() error : ");
        exit(1);
    }
    else{
        printf("-------------rcv msg-------------\n%s\n",rcv_msg);
        printf("----------------------------------\n");

        char *method=strtok(rcv_msg," ");
        char *uri=strtok(NULL," ");
        char *version=strtok(NULL,"\n");

        if(method==NULL||uri==NULL)
            state=400;
        else if(strstr(version,"HTTP/1.1")==NULL&&strstr(version,"HTTP/1.0")==NULL)
            state=400;
        else if((file_name=strtok(uri,"/"))==NULL)
             state=404;
        else 
            state=200;

        switch(state){
            case 200:
                fp=fopen(file_name,"r");
                if(fp==NULL){
                    make_header(header,"404 Not Found","text/html",strlen(HTML_404));
                    strcpy(msg,header);
                    strcat(msg,HTML_404);
                }
               else{
                    check_content_type(cnt_type,file_name);
                    
                    fseek(fp,0,SEEK_END);
                    f_len=ftell(fp);
                    fseek(fp,0,SEEK_SET);

                    char* f_msg=(char*)malloc(sizeof(char)*f_len);
                    fread(f_msg,f_len,1,fp);

                    make_header(header,"200 OK",cnt_type,f_len);
                    strcpy(msg,header);
                    strcat(msg,f_msg);
                }
                break;
            case 400:
                make_header(header,"400 Bad Request","text/html",strlen(HTML_400));
                strcpy(msg,header);
                strcat(msg,HTML_400);
                break;
            case 404:
                make_header(header,"404 Not Found","text/html",strlen(HTML_404));
                strcpy(msg,header);
                strcat(msg,HTML_404);
                break;
            case 500:
                make_header(header,"500 Internal Server Error","text/html",strlen(HTML_500));
                strcpy(msg,header);
                strcat(msg,HTML_500);
                break;
        } 
        printf("send msg:\n%s\n",msg);
        write(clnt_sock,msg,strlen(msg));
    }
    close(clnt_sock);
}
void make_header(char* header,char* state,char* cnt_type,long len){
    char* head="HTTP/1.1 ";
    char* content_type="\r\nContent-Type: ";
    char* server="\r\nServer: Localhost HTTP Server";
    char* content_len="\r\nContent-Length: ";
    char* date="\r\nDate: ";
    char* newLine="\r\n\r\n";
    char cnt_len[10];

    time_t t;
    time(&t);

    sprintf(cnt_len,"%ld",len);

    if(header!=NULL){
        strcpy(header,head);
        strcat(header,state);
        strcat(header,content_type);
        strcat(header,cnt_type);
        strcat(header,server);
        strcat(header,content_len);
        strcat(header,cnt_len);
        strcat(header,date);
        strcat(header,(char*)ctime(&t));
        strcat(header,newLine);
    }
}
void check_content_type(char* cnt_type,char* uri){
    char* extension=strrchr(uri,'.');

    if(extension==NULL){
        strcpy(cnt_type,"text/plain");
        return;
    }
    if(!strcmp(extension,".html"))
        strcpy(cnt_type,"text/html");
    else if(!strcmp(extension,".css"))
        strcpy(cnt_type,"text/css");
    else  
        strcpy(cnt_type,"text/plain");
};