#include "epoll.h"
#include "tlvpacket.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <map>

#define MAX_LISTEN_QUE 10
#define BUFFER_SIZE 1024
#define STOP 0
#define START 1

#define ASSERT(exep) ((exep)?({void(0);}):({return 1;}))
#define IPADDR "127.0.0.1"
#define PORT 8888

int Socket();
int Bind(const char *ip, const int port);
int Listen(int listen_que);
void Run();
void Over();

void Accept();
void Stdin();
void Read(int fd);
void Write(int fd, char *buf);

int listenfd = -1, status = STOP;
Epoll ep;
std::map<int, sockaddr_in> mmap;

int main(int argc, char **argv) {
    int ret = Socket();
    ASSERT(!ret);
    puts("socket create success...");

    ret = Bind(IPADDR, PORT);
    ASSERT(!ret);
    printf("socket bind with %s:%d success...\n", IPADDR, PORT);

    ret = Listen(10);
    ASSERT(!ret);
    puts("socket listen success..");

    ret = ep.create(5);
    ASSERT(!ret);
    puts("epoll create success...");

    ep.add_event(0, EPOLLIN);
    ep.add_event(listenfd, EPOLLIN);

    Run();
    Over();
    
    return 0;
}

void Run() {
    status = START;
    while(status == START) {
        int ret = ep.wait();
        for(int i = 0; i < ret; i++) {
            epoll_event *event = ep.get_event(i);
            if(event->data.fd == listenfd) {
                Accept();
            } else if(event->data.fd == 0) {
                Stdin();
            } else if(event->events & EPOLLIN) {
                Read(event->data.fd);
            }
        }
    }
}

int Listen(int listen_que) {
    int ret = listen(listenfd, listen_que);
    ASSERT(ret != -1);

    return  0;
}

int Bind(const char *ip, const int port) {
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    int ret = inet_aton(ip, &addr.sin_addr);
    ASSERT(ret != -1);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    ret = bind(listenfd, (sockaddr*)&addr, sizeof(addr));
    ASSERT(ret != -1);

    return 0;
}

int Socket() {
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    ASSERT(listenfd >= 0);

    int reuse = 1;
    int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    ASSERT(ret != -1);

    return 0;
}

void Accept() {
    int clientfd;
    sockaddr_in clientaddr;
    socklen_t clientaddr_length;
    clientfd = accept(listenfd, (sockaddr*)&clientaddr, &clientaddr_length);
    mmap.insert({clientfd, clientaddr});
    ep.add_event(clientfd, EPOLLIN);

    printf("new client \033[01;32m%d\033[0m [%s:%d] has connected.\n", clientfd, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
}

void Stdin() {
    char buf[BUFFER_SIZE];
    int len = read(0, buf, BUFFER_SIZE-1);
    if(len <= 0) return;
    buf[len-1] = '\0';   //erase \n

    if(buf[0] == 'Q' || buf[0] == 'q') Over();
    else {
        printf("recv from stdin: %s\n", buf);
    }
}

void Read(int fd) {
    TLVPacket packet;
    int len = read(fd, packet.data, BUFFER_SIZE);
    if(len < 0) return;
    if(len == 0) {
        printf("clent \033[01;32m%d\033[0m [%s:%d] has left.\n", fd, inet_ntoa(mmap[fd].sin_addr), ntohs(mmap[fd].sin_port));
        close(fd);
        mmap.erase(mmap.find(fd));
        return;
    }

    printf("recv from client \033[01;32m%d\033[0m [%s:%d] a msg{type:%s;length:%d}: %s.\n", fd, inet_ntoa(mmap[fd].sin_addr), ntohs(mmap[fd].sin_port), prase(packet.node.type), packet.node.length, packet.node.value);
}

void Over() {
    if(status == STOP) return;
    status = STOP;
    if(listenfd > 0) {
        shutdown(listenfd, SHUT_RDWR);
    }
}