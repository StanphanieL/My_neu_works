/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh 
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h" //�ṩ��һЩ����ĺ������ඨ��
 
//��������ԭ�� 
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

//��������в���������һ�������׽��֣�Ȼ����һ������ѭ���н��ܿͻ��˵��������󣬲�����doit��������ÿһ������
int main(int argc, char **argv) 
{
	//����һЩ���������ڼ����׽��֡������׽��֡��ͻ��˵��������Ͷ˿ںš��ͻ��˵ĵ�ַ���Ⱥͽṹ�� 
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //��������в����Ƿ���ȷ��ֻ��Ҫһ�����������˿ں� 
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

	//����Open_listenfd����������һ�������׽��֣��󶨵�ָ���Ķ˿� 
    listenfd = Open_listenfd(argv[1]);
    
    //����һ������ѭ�����ȴ��ͻ��˵��������� 
    while (1) {
    //��ʼ���ͻ��˵ĵ�ַ����Ϊ��ṹ��Ĵ�С 
	clientlen = sizeof(clientaddr);
	//����Accept����������һ���������󣬲�����һ�������׽��� 
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
	
	//���� Getnameinfo���������ݿͻ��˵ĵ�ַ�ṹ�壬��ȡ���������Ͷ˿ں� 
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
    //��ӡ�����յ��������������Դ��Ϣ 
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
    //����doit������������������� 
	doit(connfd);                                             //line:netp:tiny:doit
	
	//�ر������׽��� 
	Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */

//����һ��HTTP����/��Ӧ���񣬶�ȡ�����к�����ͷ������URL���ж��Ǿ�̬���Ƕ�̬���ݣ�����ļ��Ƿ���ںͿɶ�/��ִ�У�Ȼ�������Ӧ�ĺ������ṩ��̬��̬���� 
void doit(int fd) 
{
	//����һЩ���������ڴ洢�����Ƿ�Ϊ��̬���ݡ��ļ���״̬��Ϣ�������������󷽷���URI��ͳһ��Դ��λ�������汾���ļ�����CGI��ͨ�����ؽӿڣ������� 
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio; 

    /* Read request line and headers */
    
    //��ʼ��һ���������ṹ�壬�������׽��� 
    Rio_readinitb(&rio, fd);
    
    //�Ӷ��������ж�ȡһ�����ݵ�buf�У������ȡʧ���򷵻� 
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    
    //��ӡ��ȡ�������� 
    printf("%s", buf);
    
    //��buf�н��������󷽷���URI�Ͱ汾�����洢����Ӧ�ı����� 
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    
    //�Ƚ����󷽷��Ƿ�ΪGET�������� �������clienterror���� ������һ��501������Ӧ���ͻ��ˣ���ʾ��֧�ָ÷��� 
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }
	
	//��ȡ����������ͷ                                           //line:netp:doit:endrequesterr
    read_requesthdrs(&rio);                              //line:netp:doit:readrequesthdrs


    /* Parse URI from GET request */
    
    //����URI������ָ�Ϊ�ļ�����CGI����������0��ʾ��̬���ݣ�����1��ʾ��̬���� 
    is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck
    //����stat������ȡ�ļ���״̬��Ϣ������ļ������ڣ������clienterroe����������һ��404������Ӧ���ͻ��ˣ� ��ʾ�Ҳ������ļ� 
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	return;
    }                                                    //line:netp:doit:endnotfound

	//���������Ǿ�̬���ݣ���ִ�����´��� 
    if (is_static) { /* Serve static content */      
	//����ļ��Ƿ�Ϊ��ͨ�ļ��������Ƿ����û���ȡȨ�ޣ���������������������clienterror����������һ��403������Ӧ���ͻ��ˣ���ʾ��ֹ���ʸ��ļ�    
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file");
	    return;
	}
	
	//����serve_static�������ṩ��̬���ݸ��ͻ��� 
	serve_static(fd, filename, sbuf.st_size);        //line:netp:doit:servestatic
    }
    
    //���������Ƕ�̬���ݣ���ִ�����´��� 
    else { /* Serve dynamic content */
    //����ļ��Ƿ�Ϊ��ͨ�ļ��������Ƿ������û�ִ��Ȩ�ޣ� ��������������������clienterror����������һ��403������Ӧ���ͻ��ˣ���ʾ��ֹ���и�CGI����    
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
	//�ṩ��̬���ݸ��ͻ��� 
	serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */

// ��ȡHTTP����ͷ������ӡ����׼��� 
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

	//�Ӷ���������ȡһ�����ݵ�buf�� 
    Rio_readlineb(rp, buf, MAXLINE);
    //��ӡ����ȡ�������� 
    printf("%s", buf);
    
    //����ȡ�������ݲ�Ϊ��ʱ��������ȡ��һ�����ݣ�����ӡ���� 
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    
    //��ȡ��Ϻ󷵻� 
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */

 
//����URI������ָ�Ϊ�ļ�����CGI����������0��ʾ��̬���ݣ�����1��ʾ��̬���� 
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

	//���URI���Ƿ����cgi-bin�ַ�������������������ʾ������Ǿ�̬���� 
    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
    //��CGI������Ϊ���ַ��� 
	strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
	//���ļ�������Ϊ��ǰĿ¼ 
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
	//��URI���ӵ��ļ������� 
	strcat(filename, uri);                           //line:netp:parseuri:endconvert1
	
	//���URI��"/"��β�����������һ��Ŀ¼����ô�ͽ�Ĭ�ϵ���ҳ�ļ���home.html���ӵ��ļ������� 
	if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
	    strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
	
	//����1 ����ʾ����ľ�̬���� 
	return 1;
    }
    
    //���URI�а���cgi-bin�ַ��� ���ʾ���Ƕ�̬���� 
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
    
    //����index����������URI���Ƿ���"?"�ַ�������У����ʾ��CGI���� 
	ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
	if (ptr) {
		//��?������ַ������Ƶ�CGI������ 
	    strcpy(cgiargs, ptr+1);
	    //��"?"�ַ��滻Ϊ�ַ���������"\0",����URI��ֻ�������ļ������� 
	    *ptr = '\0';
	}
	//���û��"?"�ַ�
	else 
	//��CGI������Ϊ���ַ���  
	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
	    
	//���ļ�������Ϊ��ǰĿ¼ 
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
	//��URI���ӵ��ļ������� 
	strcat(filename, uri);                           //line:netp:parseuri:endconvert2
	
	//����0����ʾ������Ƕ�̬���� 
	return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */

//�ṩ��̬���ݸ��ͻ��ˣ������ļ����ͷ�����Ӧͷ����Ӧ�� 
void serve_static(int fd, char *filename, int filesize)
{
	//����һЩ���������ڴ洢Դ�ļ�����������ӳ���ַ���ļ����͡��������� 
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    //�����ļ�����ȡ�ļ����ͣ����洢��filetype�� 
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    
    //��HTTP��Ӧ��д�뵽buf�У���ʾ�ɹ���״̬��200 OK 
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    //��buf�е�����д�뵽�����׽����У����͸��ͻ��� 
    Rio_writen(fd, buf, strlen(buf));
    
    //��HTTP��Ӧͷд�뵽buf�У���ʾ������������Ϊ Tiny Web Server
    sprintf(buf, "Server: Tiny Web Server\r\n");
    // ��buf�е�����д�뵽�����׽����У����͸��ͻ���
    Rio_writen(fd, buf, strlen(buf));
    
    //��HTTP��Ӧͷд�뵽buf�У���ʾ��Ӧ��ĳ���Ϊ���� 
    sprintf(buf, "Content-length: %d\r\n", filesize);
    // ��buf�е�����д�뵽�����׽����У����͸��ͻ���
    Rio_writen(fd, buf, strlen(buf));
    
    //��HTTP��Ӧͷд�뵽buf�У���ʾ��Ӧ�������Ϊ ���� 
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    //��ֻ��ģʽ��Դ�ļ���������һ�������� 
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    
    //��Դ�ļ�ӳ�䵽һ�������ڴ�ռ䣬������һ��ָ��srcpָ��ÿռ� 
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    //�ر�Դ�ļ��������� 
    Close(srcfd);                       //line:netp:servestatic:close
    //��srcpָ�������д�뵽�����׽����У����͸��ͻ��� 
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write
    //ȡ���ڴ�ӳ�� 
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
 
//�����ļ�������չ������ȡ�ļ����� �����洢��filetype�� 
void get_filetype(char *filename, char *filetype) 
{
	//����ļ����Ƿ����.html�ַ�������ǣ����ļ���������Ϊ text/html
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
	
	//����ļ����Ƿ����.gif�ַ�������ǣ����ļ���������Ϊ image/gif
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
	
	//����ļ����Ƿ����.png�ַ�������ǣ����ļ���������Ϊ image/png
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
	
	//����ļ����Ƿ����.jpg�ַ�������ǣ����ļ���������Ϊ image/jpeg
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
	
	//��������ǣ��� ���ļ���������Ϊ text/plain
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */

//�ṩ��̬���ݸ��ͻ��� 
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
	//����һЩ���������ڴ洢����������ָ������� 
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    //��HTTP��Ӧ��д�뵽buf�У���ʾ�ɹ���״̬��200 OK 
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    //��buf�е�����д�뵽�����׽����У����͸��ͻ��� 
    Rio_writen(fd, buf, strlen(buf));
    
    //��HTTP��Ӧͷд�뵽buf�У���ʾ������������ ΪTiny Web Server 
    sprintf(buf, "Server: Tiny Web Server\r\n");
    //��buf�е�����д�뵽�����׽����У����͸��ͻ��� 
    Rio_writen(fd, buf, strlen(buf));
  
  
    //����Fork��������һ���ӽ��� 
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
	//���ӽ����У�����setenv���������û�������QUERY_STRINGΪCGI�Ĳ��� 
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
	//����׼����ض��������׽��� 
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	//ִ��CGI���򣬲����ݿ�ָ������ͻ����������� 
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    //�ڸ������У��ȴ��ӽ��̽��� 
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */


//����һ��������Ӧ���ͻ��� 
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
	// ����һ������������ 
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    // ����sprintf��������HTTP��Ӧ��д�뵽buf�У���ʾ�����״̬��ͼ����Ϣ
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    // ����Rio_writen��������buf�е�����д�뵽�����׽����У����͸��ͻ���
    Rio_writen(fd, buf, strlen(buf));
    
    // ����sprintf��������HTTP��Ӧͷд�뵽buf�У���ʾ��Ӧ�������Ϊtext/html
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    // ����Rio_writen��������buf�е�����д�뵽�����׽����У����͸��ͻ��� 
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    // ����sprintf��������HTML��ǩ�ͱ���д�뵽buf��
    sprintf(buf, "<html><title>Tiny Error</title>");
    // ����Rio_writen��������buf�е�����д�뵽�����׽����У����͸��ͻ��� 
    Rio_writen(fd, buf, strlen(buf));
    
    // ����sprintf��������HTML��ǩ�ͱ�����ɫд�뵽buf��
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    // ����Rio_writen��������buf�е�����д�뵽�����׽����У����͸��ͻ���
    Rio_writen(fd, buf, strlen(buf));
    
    // ����sprintf�������������״̬��ͼ����Ϣд�뵽buf��
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    //����Rio_writen��������buf�е�����д�뵽�����׽����У����͸��ͻ���
    Rio_writen(fd, buf, strlen(buf));
    
    // ����sprintf������������ĳ���Ϣ��ԭ��д�뵽buf��
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    // ����Rio_writen��������buf�е�����д�뵽�����׽����У����͸��ͻ���
    Rio_writen(fd, buf, strlen(buf));
    
    // ����sprintf��������HTML��ǩ�ͷ�����������д�뵽buf�� 
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    
    // ����Rio_writen��������buf�е�����д�뵽�����׽����У����͸��ͻ��� 
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */

/*
get_filetype������
�����ļ�������չ������ȡ�ļ����ͣ����洢��filetype�С��������֧�����ֳ������ļ����ͣ�html��gif��png��jpg��
����ļ����Ͳ����������֣���Ĭ��Ϊtext/plain��

serve_dynamic�������ṩ��̬���ݸ��ͻ��ˡ�����������ȷ���һ���ɹ�����Ӧͷ���ͻ��ˣ�Ȼ�󴴽�һ���ӽ��̣�
���ӽ��������û�������QUERY_STRINGΪCGI��������ִ��CGI�����ڸ������еȴ��ӽ��̽�����

clienterror����������һ��������Ӧ���ͻ��ˡ�����������ݴ���Ĳ���������һ���������Ӧͷ����Ӧ�壬�����͸��ͻ��ˡ�
��Ӧ����һ��HTMLҳ�棬���������״̬�롢�����Ϣ������Ϣ��ԭ��
*/

/*
1�����������յ�һ��HTTP����ʱ���������doit���������������
2��doit�������ȡ�����к�����ͷ�������������󷽷���URI�Ͱ汾��
3��������󷽷�����GET����������ᷢ��һ��501������Ӧ���ͻ��ˣ������ء�
4��������󷽷���GET��������������parse_uri����������URI�����ָ�Ϊ�ļ�����CGI������
5�������������ļ��Ƿ���ڣ���������ڣ�����һ��404������Ӧ���ͻ��ˣ������ء�
6�����������ж�������Ǿ�̬���ݻ��Ƕ�̬���ݣ�����Ǿ�̬���ݣ������serve_static�������ṩ��̬���ݣ�����Ƕ�̬���ݣ������serve_dynamic�������ṩ��̬���ݡ�
7��serve_static����������ļ�����ȡ�ļ����ͣ�������һ���ɹ�����Ӧͷ����Ӧ����ͻ��ˡ���Ӧ�����ļ������ݣ�ͨ���ڴ�ӳ������ȡ�ͷ��͡�
8��serve_dynamic�����ᷢ��һ���ɹ�����Ӧͷ���ͻ��ˣ�������һ���ӽ��̣����ӽ��������û�������QUERY_STRINGΪCGI��������ִ��CGI�����ڸ������еȴ��ӽ��̽�����
9��clienterror��������ݴ���Ĳ���������һ��������Ӧ���ͻ��ˡ���Ӧ����һ��HTMLҳ�棬���������״̬�롢�����Ϣ������Ϣ��ԭ��
*/
