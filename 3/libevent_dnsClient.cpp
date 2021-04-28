/************************************************************************
File name - dnsClient.cpp
Purpose - An dns client, which accepts  domain name from keyboard and \
            print it's ip.
            Reactor framework (libevent) is used.
To compile - gcc dnsClient.cpp -o client
To run - ./client 
notice - variable hostip = "202.112.14.21"; need to be same with the output
         of the command of "cat /etc/resolv.conf"
************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <event.h>
#include<event2/util.h> 


#define BUFSIZE 512
#define MAXSERVERNAME 100

typedef struct //DNS message header
{
    unsigned short id;
    unsigned short flags;
    unsigned short questNum;
    unsigned short answerNum;
    unsigned short authorNum;
    unsigned short additionNum;
} DNSHDR, *pDNSHDR;

typedef struct //DNS message request recored
{
    unsigned short type;
    unsigned short queryclass;
} QUERYHDR, *pQUERYHDR;

typedef struct //DNS message response recored
{
    unsigned short type;
    unsigned short classes;
    unsigned int ttl;
    unsigned short length;
} RESPONSE, *pRESPONSE;

int sendDnsPackage(FILE *, int, struct sockaddr *);
int gen_DNSrequest(pDNSHDR pDnsHdr, pQUERYHDR pQueryHdr, char *hostname, unsigned char *DNSsendBuff);
void decode_DNSResponse(unsigned char *DNSrecvBuff);
void recvDnsResponse(int sockfd, short events, void* arg) ;
int main(int C, char *argv[])
{
    int sd, ret;
    struct sockaddr_in serveraddress;
    //DNS address from /etc/resolv.conf
    char *hostip = "202.112.14.21";
    char *hostport = "53";

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (0 > sd)
    {
        perror("socket");
        exit(1);
    }

    memset(&serveraddress, 0, sizeof(serveraddress));
    serveraddress.sin_family = AF_INET;
    serveraddress.sin_port = htons(atoi(hostport));    //PORT NO
    serveraddress.sin_addr.s_addr = inet_addr(hostip); //ADDRESS

    printf("Client Starting service\n");

    ret = sendDnsPackage(stdin, sd, (struct sockaddr *)&serveraddress);
    if (0 > ret)
    {
        printf("Client Exiting - Some error occured\n");
        close(sd);
        exit(1);
    }
    
    struct event_base* base = event_base_new();
    //when sd has data to read
    struct event *ev_sockfd = event_new(base, sd,  
                                        EV_READ | EV_PERSIST,  
                                        recvDnsResponse, NULL);
    event_add(ev_sockfd, NULL);
    event_base_dispatch(base);  
    printf("finished \n"); 
    close(sd);
    exit(0);
    return 0;
}

/*********************************************************************
Function: - dnsProcess 
(Read the domain name from the keyboard, 
package it into a DNS request and send it to the DNS server, 
and resolve the DNS server reply, 
print the corresponding ip of the domain name)
Input - 
	a)input data source(here taken as stdin).
	b)socket descriptor.
	c)socket address structure, containing IP address and port number 
	of the server.

Returns - 
	0 or -1 based on whether it exited successfully, when 
	there was end of file(means the user stopped sending data) or 
	if any error condition was encountered.
**********************************************************************/

int sendDnsPackage(FILE *fp, /*Here to be used as stdin as argument*/
               int sockfd,
               struct sockaddr *to) /*Connection Socket */
{

    char domain_name[BUFSIZE],  servername[MAXSERVERNAME];
    char *cptr;
    int ret, numbytes, namelen, reqlen;


    DNSHDR DnsHdr;
    QUERYHDR QueryHdr;

    unsigned char DNSsendBuff[BUFSIZE];

    printf("Enter Data For the server or press CTRL-D to exit\n");
    /*Reading domain name from the keyboard*/
    cptr = fgets(domain_name, BUFSIZE, fp);
    if (NULL == cptr)
    {
        printf("Possible error or end of file\n");
        return 0;
    }
    /*Prepapre DNS request message*/
    reqlen = gen_DNSrequest(&DnsHdr, &QueryHdr, domain_name, DNSsendBuff);

    /*Sending the read data over socket*/
    ret = sendto(sockfd, DNSsendBuff, reqlen, 0, to, sizeof(struct sockaddr_in));
    if (0 > ret)
    {
        perror("Error in sending data:\n");
        return -1;
    }
    printf("Data Sent To Server\n");

    
    return 0;
}
void recvDnsResponse(int sockfd, short events, void* arg)  {
    socklen_t structlen;
    struct sockaddr_in serveraddr;
    char recvbuf[BUFSIZE];
    int numbytes;
    structlen = sizeof(serveraddr);
    memset(recvbuf, 0, sizeof(recvbuf));
    numbytes = recvfrom(sockfd, recvbuf, BUFSIZE, 0, (struct sockaddr *)&serveraddr, &structlen);
    if (0 > numbytes)
    {
        perror("Error in receiving data:\n");
        return;
    }
    printf("Data Received from server \n");
    /*decode DNS Response*/
    decode_DNSResponse((unsigned char *)recvbuf);
    return ;
}
int gen_DNSrequest(pDNSHDR pDnsHdr, pQUERYHDR pQueryHdr, char *hostname, unsigned char *DNSsendBuff)
{
    if (!strcmp(hostname, "exit"))
    {
        return -1; //input exit finish
    }
    else //正常的DNS查询请求
    {
        int iSendByte = 0;
        memset(DNSsendBuff, 0, sizeof(DNSsendBuff));
        pDnsHdr->id = htons(0x0000);        //"标识"字段设置为0
        pDnsHdr->flags = htons(0x0100);     //"标志"字段设置为0x0100, 即RD位为1期望递归查询
        pDnsHdr->questNum = htons(0x0001);  //1个查询记录
        pDnsHdr->answerNum = htons(0x0000); //没有回答记录和其它的记录
        pDnsHdr->authorNum = htons(0x0000);
        pDnsHdr->additionNum = htons(0x0000);
        //将生成的DNS查询报文首部复制到DNSsendBuff中
        memcpy(DNSsendBuff, pDnsHdr, sizeof(DNSHDR));
        iSendByte += sizeof(DNSHDR); //记录当前的数据量

        //对域名字符串进行解析并且进行形式的变换
        char *pTrace = hostname;
        char *pNumber = hostname;
        int iStrLen = strlen(hostname);
        unsigned char iCharNum = 0;

        //读取域名时若将\n读入，去除
        if (hostname[iStrLen - 1] == '\n')
        {
            hostname[iStrLen - 1] = 0;
            iStrLen -= 1;
        }
        //使句子长度包括\0
        iStrLen += 1;

        //将指针指向的字符串向后移动一个字节
        while (*pTrace != '\0')
        {
            pTrace++;
        }
        while (pTrace != hostname)
        {
            *(pTrace + 1) = *pTrace;
            pTrace--;
        }

        *(pTrace + 1) = *pTrace; //将第一个字符移动到第二个字符的位置
        pTrace++; //第一个字符无实际意义，移动到第二个字符位置

        //报文中字符域名需要调整，如"baidu.com\0"需要调整为"5baidu3com0"
        while (*pTrace != '\0')
        {
            if (*pTrace == '.')
            {
                *pNumber = iCharNum;
                pNumber = pTrace;
                iCharNum = 0;
            }
            else
            {
                iCharNum++;
            }
            pTrace++;
        }
        *pNumber = iCharNum;

        memcpy(DNSsendBuff + sizeof(DNSHDR), hostname, iStrLen + 1);
        iSendByte += (iStrLen + 1); //解析完之后会多出1个字符

        //在域名字段之后填入“查询类型”和“查询类”
        pQueryHdr->type = htons(0x0001);
        pQueryHdr->queryclass = htons(0x0001);
        memcpy(DNSsendBuff + sizeof(DNSHDR) + (iStrLen + 1), pQueryHdr, sizeof(QUERYHDR));

        iSendByte += sizeof(QUERYHDR); // 累加得到的字节数
        return iSendByte;              //返回最终得到的字节数
    }
}

void decode_DNSResponse(unsigned char *DNSrecvBuff)
{
    if (DNSrecvBuff == NULL)
    {
        perror("No messge received\n");
    }
    pDNSHDR pDnsHdr = (pDNSHDR)DNSrecvBuff; //指针pDnsHdr指向接收到的DNS应答报文首部

    //保存所有附加信息
    int iQueryNum, iRespNum, iAuthRespNum, iAdditionNum;
    iQueryNum = ntohs(pDnsHdr->questNum);
    iRespNum = ntohs(pDnsHdr->answerNum);
    iAuthRespNum = ntohs(pDnsHdr->authorNum);
    iAdditionNum = ntohs(pDnsHdr->additionNum);

    //将DNS应答报文的“标志”字段右移15位即取最高位 ， 0 为DNS查询报文，1为应答报文
    //由于网络字节序字节15变成7位
    if (pDnsHdr->flags >> 7)
    //
    {
        //flags低位值为3，标识服务器没有与请求域名相应的记录
        if (((pDnsHdr->flags >> 4) & 0x0007) != 0)
        {
            printf("No corresponding domain name entry . \n");
            return;
        }
        if ((pDnsHdr->flags >> 2) & 0x0001) //查看标志位AA，看是否时权威应答
        {
            printf("Authoritative anwser : \n");
        }
        else
        {
            printf("None-authoritative anwser : \n");
        }

        unsigned char *pTraceResponse;
        //指针移向应答报文中的第一个查询记录，因为一般情况下应答报文均会首先附带一个对应的查询记录
        pTraceResponse = DNSrecvBuff + sizeof(DNSHDR);

        //将指针移动到查询记录的域名字段之后
        while (*pTraceResponse)
        {
            pTraceResponse++;
        }
        pTraceResponse++;
        //跳过查询类型和查询类两个字段，指针指向第一个应答记录
        pTraceResponse += (sizeof(short) * 2);
        struct in_addr address;
        pRESPONSE pResponse;
        for (int i = 0; i < iRespNum; i++)
        {
            //指针跳过应答记录的“域名”字段，此域名字段一般为一个域名指针，以0xC0开始
            pTraceResponse += sizeof(short);
            pResponse = (pRESPONSE)pTraceResponse;
            if (ntohs(pResponse->type) == 1) //这条应答记录返回的是与之前查询所对应的IP地址
            {
                //更加简洁地，这里应该使用 pTraceResponse += (sizeof(Response))
                //然而，由于结构数据对齐，sizeof(Response) > sizeof(Response内部成员大小之和)
                //若使用pTraceResponse += (sizeof(Response))，应使用__attribute__ ((packed))
                pTraceResponse += (sizeof(pResponse->classes) + sizeof(pResponse->type) + sizeof(pResponse->ttl) + sizeof(pResponse->length));
                unsigned int ulIP = *(unsigned int *)pTraceResponse;
                address.s_addr = ulIP;
                if (i == iRespNum - 1) //最后一条记录显示句号，否则显示分号
                {
                    printf("%s . ", inet_ntoa(address));
                }
                else
                {
                    printf("%s ; ", inet_ntoa(address));
                }

                //指针移过应答记录的IP地址字段，指向下一条应答记录
                pTraceResponse += sizeof(int);
            }
            else if (ntohs(pResponse->type) == 5) //这条应答记录为所查询主机的一个别名，这里本程序直接跳过这条记录
            {
                pTraceResponse += sizeof(RESPONSE);
                pTraceResponse += ntohs(pResponse->length);
            }
        }
        printf("\n");
    }
    else //标志字段最高位不为1，表示不是一个DNS应答报文，不做任何处理
    {
        printf("Invalid DNS resolution ! \n\n");
    }
}
