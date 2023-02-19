//헤더파일
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h> 

//에러시 보낼 HTML 문자열
char* HTML_400="<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>P4C Week3 HTTP Server</title></head><body><h1>400 Bad Request</h1></body></html>";
char* HTML_404="<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>P4C Week3 HTTP Server</title></head><body><h1>404 Not Found</h1></body></html>";
char* HTML_500="<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>P4C Week3 HTTP Server</title></head><body><h1>500 Internal Server Error</h1></body></html>";

//클라이언트 요청 처리할 함수
void *handle_client(void* clnt_socket);
//클라이언트에게 보낼 헤더 만드는 함수
void make_header(char* header,char* state,char* cnt_type,long len);
//클라이언트에게 받은 요청 중 파일의 타입이 무엇인지 정할 함수
void check_content_type(char* cnt_type,char* uri);

int main(int argc,char** argv) {
    int serv_sock,clnt_sock; //서버 소켓과 클라이언트 소켓
    struct sockaddr_in serv_addr,clnt_addr;//서버,클라이언트의 프로토콜 체계, 소켓 타입, 포트 번호 등을 담을 구조체
    int clnt_addr_size;//클라이언트 주소의 크기, accept() 할 때 쓰임
    pthread_t thread; //멀티쓰레드를 위한 스레드 변수

    //만약 입력 창에 포트 번호가 입력이 안되었다면 출력하고 프로그램을 종료한다.
    if(argc!=2){ 
        printf("Input : %s <port>\n",argv[0]);
        exit(1); 
    }
   
    //서버 소켓 생성, PF_iNET : IPv4, SOCK_STREAM : TCP, 0 : protocol
    if((serv_sock=socket(PF_INET,SOCK_STREAM,0))==-1){
        perror("socket() error : "); //소켓 생성 실패시 소켓 에러 출력하고 프로그램을 종료한다.
        exit(1);
    }
    else
        printf("socket() success\n"); //소켓 생성 성공 시 확인용으로 출력.

    //소켓 주소 정보 초기화
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;//sin_family-> 주소체계, AF_INET -> IPv4
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);//sin_addr.s_addr -> IP 주소 정보 , INADDR_ANY -> 이 컴퓨터에 존재하는 랜 카드중 사용 가능한 랜카드의 IP주소 자동 대입
    serv_addr.sin_port=htons(atoi(argv[1]));//sin_port -> 포트 번호, argv[1]-> 입력 받은 포트 번호

    //bind()를 통해 서버 소켓과 위의 주소 정보 연결
    if(bind(serv_sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr))==-1){
        perror("bind() error : "); //bind() 실패 시 bind() 에러 출력하고 프로그램을 종료한다.
        exit(1);
    }
    else                           
        printf("bind() success\n");//bind() 성공 시 확인용으로 출력.

    //listen()을 통해 대기열 20개 만들고 클라이언트 요청 대기
    if(listen(serv_sock,20)==-1){
        perror("listen() error : "); //listen() 실패 시 에러 출력하고 프로그램을 종료한다.
        exit(1);
    }
    else 
        printf("listen() success\n");//listen() 성공 시 확인용으로 출력.

    //반복문을 돌며 클라이언트의 요청을 승낙할 것이다.
    while(1){
        clnt_addr_size=sizeof(clnt_addr); //클라이언트 주소 크기를 구해준다. 밑의 accept()에서 사용할 것이다.
        clnt_sock=accept(serv_sock,(struct sockaddr*)&clnt_addr,&clnt_addr_size);//accept()를 통해 클라이언트와 서버를 연결한다
        if(clnt_sock==-1){
            perror("accept() errork : ");//accept() 실패 시 에러 출력하고 프로그램을 종료한다,
            exit(1);
        }
        //확인용으로 클라이언트의 IP 주소와 포트 번호를 출력한다.
        printf("connection request <ip> %s <port>%d\n",inet_ntoa(clnt_addr.sin_addr),ntohs(clnt_addr.sin_port));
        //멀티 스레드를 위해 스레드를 생성한다. 또한 클라이언트 요청 처리를 위해 handle_client()로 클라이언트 소켓을 넘겨준다.
        pthread_create(&thread,NULL,handle_client,&clnt_sock);     
    
    }
    
    return 0;
}
//클라이언트의 요청을 처리할 함수이다.
void* handle_client(void *sock){ //클라이언트 소켓을 인자로 받는다.
    int clnt_sock=*(int*)sock; //인자로 받은 클라이언트의 소켓을 다시 int 형으로 바꿔준다.
    char rcv_msg[1024]; //클라이언트로부터 받을 메세지를 담을 변수
    char msg[2048]; //클라이언트에게 보낼 메세지를 담을 변수
    char* file_name; //클라이언트로부터 요청받은 파일 이름을 저장할 변수
    FILE* fp; //클라이언트로부터 요청받은 파일을 열 변수
    long f_len; //클라이언트로부터 요청받은 파일의 크기를 저장할 변수
    
    int state; //HTTP의 상태 코드를 저장할 변수

    char cnt_type[20]; //콘텐츠 타입을 저장할 변수
    char header[1024]; //클라이언트에게 보낼 헤더를 저장할 변수

    //받은 요청과 보낼 데이터를 담을 변수를 초기화 시켜준다.
    memset(rcv_msg,0,sizeof(rcv_msg));
    memset(msg,0,sizeof(msg));

    //read() 를 통해 클라이언트로부터 메세지를 받아 rcv_msg에 저장한다.
    if(read(clnt_sock,rcv_msg,sizeof(rcv_msg)-1)==-1){
        perror("read() error : "); //read() 실패 시 에러 출력하고 프로그램 종료
        exit(1);
    }
    else{ //read() 성공 시 확인용으로 받은 메세지 출력//그 뒤에는 전역변수로 선언해놓은 404 에러에 적절한 HTML 문자열(HTML_404)을 붙여준다.
        printf("-------------rcv msg-------------\n%s\n",rcv_msg);
        printf("----------------------------------\n");

        //요청 행을 쪼갤 변수들.. rcv_msg의 첫줄(요청행)을 ' '로 쪼개 저장한다.
        char *method=strtok(rcv_msg," "); //method에 HTTP 메소드 저장      ex)GET
        char *uri=strtok(NULL," ");  //uri에 클라이언트가 요청한 파일 저장  ex)/index.html  -> '/'까지 저장한 이유는 uri가 입력 안됐을 때 404에러를 보내기 위해서이다.(아래에 나옴)
        char *version=strtok(NULL,"\n"); //version에 HTTP 버전 정보 저장   ex)HTTP/1.1

        //method나 uri가 null이면 잘못된 요청이므로 state에 400을 저장한다.
        if(method==NULL||uri==NULL)
            state=400;
        //HTTP 버전이 HTTP/1.1이나 HTTP/1.0이 아니라면 잘못된 요청이므로 state에 400을 저장한다.
        else if(strstr(version,"HTTP/1.1")==NULL&&strstr(version,"HTTP/1.0")==NULL)
            state=400;//그 뒤에는 전역변수로 선언해놓은 404 에러에 적절한 HTML 문자열(HTML_404)을 붙여준다.
        //file_name에 '/'를 제거해 넣어준다. 만약 '/'를 잘랐는데 NULL이 저장되었다면 요청 파일이 없다는 것이므로 state에 404을 저장한다.
        else if((file_name=strtok(uri,"/"))==NULL)
             state=404;
        //위의 에러가 없다면 성공적으로 요청을 수행할 수 있다는 것으로 state에 200을 저장한다.
        else 
            state=200;

        //상태 코드에 따라 보낼 데이터가 다르므로 switch문으로 작성했다.
        switch(state){
            case 200: //요청을 수행할 수 있다면
                fp=fopen(file_name,"r"); //파일 오픈을 한다
                if(fp==NULL){ //만약 파일 오픈에 실패했다면 404 에러이다.(Not Found 찾을 수 없는 데이터이므로..)
                    make_header(header,"404 Not Found","text/html",strlen(HTML_404));//make_header()를 통해 404 에러에 적절한 헤더를 만들어준다.
                    strcpy(msg,header); //msg에 header(make_header를 통해 만듦)를 복사해준다.
                    strcat(msg,HTML_404);//그 뒤에는 전역변수로 선언해놓은 404 에러에 적절한 HTML 문자열(HTML_404)을 붙여준다.
                }
               else{ //파일 오픈에 성공했을 경우
                    check_content_type(cnt_type,file_name);//check_cnt_type()함수에 file_name를 넘겨줘 콘텐츠 타입을 cnt_type 변수에 저장한다. 
                    
                    //fseek을 통해 요청받은 파일의 크기를 구해 f_len에 저장한다. (헤더 만들때 사용)
                    fseek(fp,0,SEEK_END);
                    f_len=ftell(fp);
                    fseek(fp,0,SEEK_SET);

                    char* f_msg=(char*)malloc(sizeof(char)*f_len);//요청받은 파일의 크기(f_len)만큼 파일을 읽어 저장할 f_msg 변수를 만든다.
                    fread(f_msg,f_len,1,fp);//파일을 읽어 f_msg 변수에 저장한다.

                    make_header(header,"200 OK",cnt_type,f_len); //make_header()를 통해 200 OK에 적절한 헤더를 만들어준다.
                    strcpy(msg,header); //msg에 header(make_header를 통해 만듦)를 복사해준다.
                    strcat(msg,f_msg); //그 뒤에는 클라이언트가 요청한 파일을 붙여준다.
                }
                break;
            //400 에러일 경우
            case 400:
                make_header(header,"400 Bad Request","text/html",strlen(HTML_400)); //make_header()를 통해 400 에러에 적절한 헤더를 만들어준다.
                strcpy(msg,header); //msg에 header(make_header를 통해 만듦)를 복사해준다.
                strcat(msg,HTML_400); //그 뒤에는 전역변수로 선언해놓은 400 에러에 적절한 HTML 문자열(HTML_400)을 붙여준다.
                break;
            //404 에러일 경우
            case 404:
                make_header(header,"404 Not Found","text/html",strlen(HTML_404)); //make_header()를 통해 404 에러에 적절한 헤더를 만들어준다.
                strcpy(msg,header); //msg에 header(make_header를 통해 만듦)를 복사해준다.
                strcat(msg,HTML_404); //그 뒤에는 전역변수로 선언해놓은 404 에러에 적절한 HTML 문자열(HTML_404)을 붙여준다.
                break;
            //500 에러일 경우
            case 500:
                make_header(header,"500 Internal Server Error","text/html",strlen(HTML_500));//make_header()를 통해 500 에러에 적절한 헤더를 만들어준다.
                strcpy(msg,header); //msg에 header(make_header를 통해 만듦)를 복사해준다.
                strcat(msg,HTML_500); //그 뒤에는 전역변수로 선언해놓은 500 에러에 적절한 HTML 문자열(HTML_500)을 붙여준다.
                break;
        } 
        printf("send msg:\n%s\n",msg); //클라이언트에게 전송할 데이터를 확인차 콘솔에 출력한다.
        write(clnt_sock,msg,strlen(msg));//클라이언트에게 헤더와 바디를 합쳐 만든 데이터를 전송한다.
    }
    close(clnt_sock);//클라이언트 소켓 닫아줌.
}
//클라이언트에게 보낼 헤더를 만드는 함수
void make_header(char* header,char* state,char* cnt_type,long len){ //만든 헤더 저장할 변수,상태 코드와 메세지, 콘텐츠 타입, 전송할 데이터의 길이   
    //상태 행
    char* head="HTTP/1.1 "; //응답 메세지의 상태 행
    //헤더 
    char* content_type="\r\nContent-Type: "; //콘텐츠 타입
    char* server="\r\nServer: Localhost HTTP Server"; //서버 정보 
    char* content_len="\r\nContent-Length: "; //전송할 데이터의 길이
    char* date="\r\nDate: "; //오늘 날짜
    char* newLine="\r\n\r\n"; //Blank로 헤더와 바디를 구분지을 것
    char cnt_len[10]; //전송할 데이터의 길이를 string으로 바꿔 담을 변수

    time_t t;    //현재 날짜,요일,시간을 알기 위해
    time(&t);

    sprintf(cnt_len,"%ld",len); //매개변수로 받은 len의 길이를 문자열로 바꿔 cnt_len에 담는다

    //상태 행과 헤더 변수, 매개변수를 합쳐 헤더를 만들어 헤더에 저장
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
//클라이언트에게 요청받은 파일이 무슨 타입인지 알려주는 함수
void check_content_type(char* cnt_type,char* uri){ //타입 저장할 문자열, 클라이언트로부터 요청받은 파일
    char* extension=strrchr(uri,'.'); //요청받은 파일의 뒷부분을 저장해줌    ex) index.html -> .html

    if(extension==NULL){ //NULL이라면 text/plain 저장
        strcpy(cnt_type,"text/plain");
        return;
    }
    if(!strcmp(extension,".html"))     //html로 끝난다면 "text/html" 저장
        strcpy(cnt_type,"text/html");
    else if(!strcmp(extension,".css")) //css로 끝난다면 "text/css" 저장
        strcpy(cnt_type,"text/css");
    else                               //아무것도 아니면 "text/plain" 저장
        strcpy(cnt_type,"text/plain");
};