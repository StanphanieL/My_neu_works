/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh 
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h" //提供了一些方便的函数和类定义
 
//声明函数原型 
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

//检查命令行参数，创建一个监听套接字，然后在一个无限循环中接受客户端的连接请求，并调用doit函数处理每一个请求
int main(int argc, char **argv) 
{
	//定义一些变量，用于监听套接字、连接套接字、客户端的主机名和端口号、客户端的地址长度和结构体 
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    //检查命令行参数是否正确，只需要一个参数，即端口号 
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

	//调用Open_listenfd函数，创建一个监听套接字，绑定到指定的端口 
    listenfd = Open_listenfd(argv[1]);
    
    //进入一个无限循环，等待客户端的连接请求 
    while (1) {
    //初始化客户端的地址长度为其结构体的大小 
	clientlen = sizeof(clientaddr);
	//调用Accept函数，接受一个连接请求，并返回一个连接套接字 
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
	
	//调用 Getnameinfo函数，根据客户端的地址结构体，获取其主机名和端口号 
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
    //打印出接收到的连接请求的来源信息 
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        
    //调用doit函数，处理该连接请求 
	doit(connfd);                                             //line:netp:tiny:doit
	
	//关闭连接套接字 
	Close(connfd);                                            //line:netp:tiny:close
    }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */

//处理一个HTTP请求/相应事务，读取请求行和请求头，解析URL，判断是静态还是动态内容，检查文件是否存在和可读/可执行，然后调用相应的函数来提供静态或动态内容 
void doit(int fd) 
{
	//定义一些变量，用于存储请求是否为静态内容、文件的状态信息、缓冲区、请求方法、URI（统一资源定位符）、版本、文件名、CGI（通用网关接口）参数等 
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio; 

    /* Read request line and headers */
    
    //初始化一个缓冲区结构体，关联到套接字 
    Rio_readinitb(&rio, fd);
    
    //从读缓冲区中读取一行数据到buf中，如果读取失败则返回 
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    
    //打印读取到的数据 
    printf("%s", buf);
    
    //从buf中解析出请求方法、URI和版本，并存储到相应的变量中 
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    
    //比较请求方法是否为GET，若不是 ，则调用clienterror函数 ，发送一个501错误响应给客户端，表示不支持该方法 
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }
	
	//读取并忽略请求头                                           //line:netp:doit:endrequesterr
    read_requesthdrs(&rio);                              //line:netp:doit:readrequesthdrs


    /* Parse URI from GET request */
    
    //解析URI，将其分割为文件名和CGI参数，返回0表示动态内容，返回1表示静态内容 
    is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck
    //调用stat函数获取文件的状态信息，如果文件不存在，则调用clienterroe函数，发送一个404错误响应给客户端， 表示找不到该文件 
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
	clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	return;
    }                                                    //line:netp:doit:endnotfound

	//如果请求的是静态内容，则执行以下代码 
    if (is_static) { /* Serve static content */      
	//检查文件是否为普通文件，并且是否有用户读取权限，如果不满足条件，则调用clienterror函数，发送一个403错误响应给客户端，表示禁止访问该文件    
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file");
	    return;
	}
	
	//调用serve_static函数，提供静态内容给客户端 
	serve_static(fd, filename, sbuf.st_size);        //line:netp:doit:servestatic
    }
    
    //如果请求的是动态内容，则执行以下代码 
    else { /* Serve dynamic content */
    //检查文件是否为普通文件，并且是否有由用户执行权限， 如果不满足条件，则调用clienterror函数，发送一个403错误响应给客户端，表示禁止运行该CGI程序    
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
	//提供动态内容给客户端 
	serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */

// 读取HTTP请求头，并打印到标准输出 
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

	//从读缓冲区读取一行数据到buf中 
    Rio_readlineb(rp, buf, MAXLINE);
    //打印出读取到的数据 
    printf("%s", buf);
    
    //当读取到的数据不为空时，继续读取下一行数据，并打印出来 
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    
    //读取完毕后返回 
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */

 
//解析URI，将其分割为文件名和CGI参数，返回0表示动态内容，返回1表示静态内容 
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

	//检查URI中是否包含cgi-bin字符串，如果不包含，则表示请求的是静态内容 
    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
    //将CGI参数置为空字符串 
	strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
	//将文件名设置为当前目录 
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
	//将URI附加到文件名后面 
	strcat(filename, uri);                           //line:netp:parseuri:endconvert1
	
	//如果URI以"/"结尾，则请求的是一个目录，那么就将默认的主页文件名home.html附加到文件名后面 
	if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
	    strcat(filename, "home.html");               //line:netp:parseuri:appenddefault
	
	//返回1 ，表示请求的静态内容 
	return 1;
    }
    
    //如果URI中包含cgi-bin字符串 则表示的是动态内容 
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
    
    //调用index函数，查找URI中是否有"?"字符，如果有，则表示有CGI参数 
	ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
	if (ptr) {
		//将?后面的字符串复制到CGI参数中 
	    strcpy(cgiargs, ptr+1);
	    //将"?"字符替换为字符创结束符"\0",这样URI就只包含了文件名部分 
	    *ptr = '\0';
	}
	//如果没有"?"字符
	else 
	//将CGI参数置为空字符串  
	    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
	    
	//将文件名设置为当前目录 
	strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
	//将URI附加到文件名后面 
	strcat(filename, uri);                           //line:netp:parseuri:endconvert2
	
	//返回0，表示请求的是动态内容 
	return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */

//提供静态内容给客户端，根据文件类型发送响应头和响应体 
void serve_static(int fd, char *filename, int filesize)
{
	//定义一些变量，用于存储源文件的描述符、映射地址、文件类型、缓冲区等 
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    //根据文件名获取文件类型，并存储到filetype中 
    get_filetype(filename, filetype);    //line:netp:servestatic:getfiletype
    
    //将HTTP响应行写入到buf中，表示成功的状态码200 OK 
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //line:netp:servestatic:beginserve
    //将buf中的数据写入到连接套接字中，发送给客户端 
    Rio_writen(fd, buf, strlen(buf));
    
    //将HTTP响应头写入到buf中，表示服务器的类型为 Tiny Web Server
    sprintf(buf, "Server: Tiny Web Server\r\n");
    // 将buf中的数据写入到连接套接字中，发送给客户端
    Rio_writen(fd, buf, strlen(buf));
    
    //将HTTP响应头写入到buf中，表示响应体的长度为。。 
    sprintf(buf, "Content-length: %d\r\n", filesize);
    // 将buf中的数据写入到连接套接字中，发送给客户端
    Rio_writen(fd, buf, strlen(buf));
    
    //将HTTP响应头写入到buf中，表示响应体的类型为 。。 
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    //line:netp:servestatic:endserve

    /* Send response body to client */
    //用只读模式打开源文件，并返回一个描述符 
    srcfd = Open(filename, O_RDONLY, 0); //line:netp:servestatic:open
    
    //将源文件映射到一个虚拟内存空间，并返回一个指针srcp指向该空间 
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //line:netp:servestatic:mmap
    //关闭源文件的描述符 
    Close(srcfd);                       //line:netp:servestatic:close
    //将srcp指向的数据写入到连接套接字中，发送给客户端 
    Rio_writen(fd, srcp, filesize);     //line:netp:servestatic:write
    //取消内存映射 
    Munmap(srcp, filesize);             //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
 
//根据文件名的扩展名，获取文件类型 ，并存储到filetype中 
void get_filetype(char *filename, char *filetype) 
{
	//检查文件名是否包含.html字符，如果是，则将文件类型设置为 text/html
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
	
	//检查文件名是否包含.gif字符，如果是，则将文件类型设置为 image/gif
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
	
	//检查文件名是否包含.png字符，如果是，则将文件类型设置为 image/png
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
	
	//检查文件名是否包含.jpg字符，如果是，则将文件类型设置为 image/jpeg
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
	
	//如果都不是，则 将文件类型设置为 text/plain
    else
	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */

//提供动态内容给客户端 
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
	//定义一些变量，用于存储缓冲区、空指针数组等 
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    //将HTTP响应行写入到buf中，表示成功的状态码200 OK 
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    //将buf中的数据写入到连接套接字中，发送给客户端 
    Rio_writen(fd, buf, strlen(buf));
    
    //将HTTP响应头写入到buf中，表示服务器的类型 为Tiny Web Server 
    sprintf(buf, "Server: Tiny Web Server\r\n");
    //将buf中的数据写入到连接套接字中，发送给客户端 
    Rio_writen(fd, buf, strlen(buf));
  
  
    //调用Fork函数创建一个子进程 
    if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
	/* Real server would set all CGI vars here */
	//在子进程中，调用setenv函数，设置环境变量QUERY_STRING为CGI的参数 
	setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
	//将标准输出重定向到连接套接字 
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
	//执行CGI程序，并传递空指针数组和环境变量数组 
	Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    //在父进程中，等待子进程结束 
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */


//发送一个错误响应给客户端 
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
	// 定义一个缓冲区变量 
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    // 调用sprintf函数，将HTTP响应行写入到buf中，表示错误的状态码和简短消息
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    // 调用Rio_writen函数，将buf中的数据写入到连接套接字中，发送给客户端
    Rio_writen(fd, buf, strlen(buf));
    
    // 调用sprintf函数，将HTTP响应头写入到buf中，表示响应体的类型为text/html
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    // 调用Rio_writen函数，将buf中的数据写入到连接套接字中，发送给客户端 
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    // 调用sprintf函数，将HTML标签和标题写入到buf中
    sprintf(buf, "<html><title>Tiny Error</title>");
    // 调用Rio_writen函数，将buf中的数据写入到连接套接字中，发送给客户端 
    Rio_writen(fd, buf, strlen(buf));
    
    // 调用sprintf函数，将HTML标签和背景颜色写入到buf中
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    // 调用Rio_writen函数，将buf中的数据写入到连接套接字中，发送给客户端
    Rio_writen(fd, buf, strlen(buf));
    
    // 调用sprintf函数，将错误的状态码和简短消息写入到buf中
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    //调用Rio_writen函数，将buf中的数据写入到连接套接字中，发送给客户端
    Rio_writen(fd, buf, strlen(buf));
    
    // 调用sprintf函数，将错误的长消息和原因写入到buf中
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    // 调用Rio_writen函数，将buf中的数据写入到连接套接字中，发送给客户端
    Rio_writen(fd, buf, strlen(buf));
    
    // 调用sprintf函数，将HTML标签和服务器的名称写入到buf中 
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    
    // 调用Rio_writen函数，将buf中的数据写入到连接套接字中，发送给客户端 
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */

/*
get_filetype函数：
根据文件名的扩展名，获取文件类型，并存储到filetype中。这个函数支持四种常见的文件类型：html、gif、png和jpg。
如果文件类型不属于这四种，则默认为text/plain。

serve_dynamic函数：提供动态内容给客户端。这个函数首先发送一个成功的响应头给客户端，然后创建一个子进程，
在子进程中设置环境变量QUERY_STRING为CGI参数，并执行CGI程序。在父进程中等待子进程结束。

clienterror函数：发送一个错误响应给客户端。这个函数根据传入的参数，构造一个错误的响应头和响应体，并发送给客户端。
响应体是一个HTML页面，包含错误的状态码、简短消息、长消息和原因。
*/

/*
1、当服务器收到一个HTTP请求时，它会调用doit函数来处理该请求。
2、doit函数会读取请求行和请求头，并解析出请求方法、URI和版本。
3、如果请求方法不是GET，则服务器会发送一个501错误响应给客户端，并返回。
4、如果请求方法是GET，则服务器会调用parse_uri函数来解析URI，并分割为文件名和CGI参数。
5、服务器会检查文件是否存在，如果不存在，则发送一个404错误响应给客户端，并返回。
6、服务器会判断请求的是静态内容还是动态内容，如果是静态内容，则调用serve_static函数来提供静态内容；如果是动态内容，则调用serve_dynamic函数来提供动态内容。
7、serve_static函数会根据文件名获取文件类型，并发送一个成功的响应头和响应体给客户端。响应体是文件的内容，通过内存映射来读取和发送。
8、serve_dynamic函数会发送一个成功的响应头给客户端，并创建一个子进程，在子进程中设置环境变量QUERY_STRING为CGI参数，并执行CGI程序。在父进程中等待子进程结束。
9、clienterror函数会根据传入的参数，发送一个错误响应给客户端。响应体是一个HTML页面，包含错误的状态码、简短消息、长消息和原因。
*/
