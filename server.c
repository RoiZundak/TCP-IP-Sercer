/*----------HTTP-server--------------
	Author: Roi Cohen
----------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <limits.h>
#include <time.h>	
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include "threadpool.h"


/*---------------------------------------------response--------------------------------------------*/
char* response302="\r\n\n<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\n<BODY><H4>302 Found</H4>\nDirectories must end with a slash.\n</BODY></HTML\r\n";

char* response400="\r\n\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H4>400 Bad request</H4>\nBad Request.\n</BODY></HTML>\r\n";

char* response403="\r\n\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>\r\n";

char* response404="\r\n\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H4>404 Not Found</H4>\nFile not found.</BODY></HTML>\r\n";

char* response500="<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H4>500 Internal Server Error</H4>\nSome server side error.\n</BODY></HTML>\r\n";
char* response501= "\r\n\n<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\n<BODY><H4>501 Not supported</H4>\nMethod is not supported.\n</BODY></HTML>\r\n";

#define FAILURE -1
#define SUCCESS  0
#define EMPTY  0
#define FILE 1
#define DIRECTORY 0
#define INDEX_HTML 1
#define port_place 1
#define p_size_place 2
#define request_place 3
#define NOACCESS 3
#define input_size 4
#define waitingSocketOnQueue 5
#define JUSTGET 14
#define MAX_REQ 4096
#define MAX_RES 1024
#define TIME_SIZE 128
#define BYTESIZE 64
#define USAGE "Usage: server <port> <poolsize> <max-number-of-request>"
#define FOUND 302
#define BADREQUEST 400
#define FORBIDDEN 403
#define NOTFOUND 404
#define INTERNAL 500
#define NOTSUPPORTED 501

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

typedef enum {false, true} bool;

/*--------------decleretion-----------------*/
bool isNumber(char*);
bool checkInput(char**,int);
void connectsocket(int,int,int);
bool setRequest(char*,int);
int dispatch_here(void*);
int buildErrMsg(char*,char*,int,int);
int kindOf(char*);
int handleDir(char*,char*,char*,int);
char* get_mime_type(char*);
int handleFile(char*,char*,char*,int);
void perorr(char*);
void handlebackslash(char *request);
/*----------------------------------------*/


/*------------main----------------*/
int main(int argc,char* argv[]){
	int port=0;
	int pool_size=0;
	int num_of_request=0;

	
	/*input sanity check */
	if(!checkInput(argv,argc))
		perorr(USAGE);
	
	port=atoi(argv[port_place]);
	pool_size=atoi(argv[p_size_place]);
	num_of_request=atoi(argv[request_place]);
		
	/*Start work!*/
   connectsocket(port ,num_of_request,pool_size);
   return 0;
}
/*----------------------------------------*/

/*--------------checkInput() & isNumber()-----------------
2 method to check the input.
if the arguments isn't 4 return false o.w check if
all argv from 1 to 4 is numbers with isNumber method
that get char * and check if the argument is number.
--------------------------------------------------------*/
bool checkInput(char * argv[],int argc){
	int i=port_place;
	if(argc!=input_size)
		return false;
	for(;i<input_size;i++)
		if(!isNumber(argv[i]))
			return false;
	return true;
}
bool isNumber(char *input){
    while (*input != '\0'){
        if (*input<'0'||*input>'9')
				return false;
        input++;
    }
    return true;
}


/*--------------------------connectsocket()--------------------------
	Function that get 3 insts:the port,number of request and the pool size.
	Strat to connect to socket by the algorithm:
	socket->bind->listen->create the pool->accept.
	After create the threadpool for loop that create the file desciptor of the socket,
	than for all of them do accept than call to-dispathch here.	
-------------------------------------------------------------------*/
void connectsocket(int port,int num_of_request,int pool_size){
	/*init variballs*/
   int server_socket=0;
   int i=0;
	struct sockaddr_in serv_addr;

	threadpool *pool;
	if ((server_socket = socket(PF_INET,SOCK_STREAM, 0))<SUCCESS)
		perorr("Socket");
	
	 memset(&serv_addr, '0', sizeof(serv_addr));
   
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port); 
    
	 if(bind(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<SUCCESS)
	 		perorr("bind"); 
	 		
	 /*5 is the number of the waiting socket on queue*/		
    if(listen(server_socket,waitingSocketOnQueue)<SUCCESS)
	 		perorr("listen"); 
	 		
	pool = create_threadpool(pool_size);
	if(!pool){
		perorr("create_threadpool");
	}
	int* fdClient;
	for(i=0;i<num_of_request;i++){
		fdClient = (int*)malloc(sizeof(int));
		if(!fdClient){
			destroy_threadpool(pool);
			close(server_socket);
			perorr("malloc");
		}
  	   *fdClient =accept(server_socket,NULL,NULL);
  	   if(*fdClient<0){
  	   	destroy_threadpool(pool);
  	   	free(fdClient);
  	   	close(server_socket);
  	   	perorr("accept");
  	   }
  	  	dispatch(pool,(dispatch_fn)dispatch_here,(void*)fdClient);
	}
	destroy_threadpool(pool);
	close(server_socket);
}
/*--------------------dispatch_here()-----------------------------*/
int dispatch_here(void* client_socket){
	int flag=0;
	int *temp = (int*)client_socket;
	int c_socket= *temp;
	free(temp);
	bool isValid=false;	
	
	char* request = (char*)malloc(sizeof(char)*(MAX_REQ));
	bzero(request,sizeof(char)*(MAX_REQ));
	
	if(!request){
			flag=buildErrMsg(NULL,NULL,INTERNAL,c_socket);			
			if(flag!=SUCCESS)
				return FAILURE;
	}
	int end_request=0;
	int read_request=1;
	while(read_request>0){
		read_request=read(c_socket,&request[end_request],MAX_REQ);
		if(read_request<0){
			close(c_socket);
			free(request);
			flag=buildErrMsg(NULL,NULL,INTERNAL,c_socket);
			if(flag!=SUCCESS){
				free(request);
				close(c_socket);
			return FAILURE;
		}
			return FAILURE;
		}
		end_request+=read_request;
		/*finish read the head*/
		if(strstr(request,"\r\n")!=NULL)
			break;
	}

	request[end_request]='\0';

	handlebackslash(request);   	
	
	isValid=setRequest(request,c_socket);
	if(isValid==false){
		flag=buildErrMsg(NULL,NULL,INTERNAL,c_socket);
		if(flag!=SUCCESS){
			free(request);
			close(c_socket);
			return FAILURE;
		}
 		free(request);
 		close(c_socket);
	}
	free(request);
	return SUCCESS;
}
/*--------------------------setRequest()-------------------------*/
bool setRequest(char* request,int client_socket){
	int flag_kind=0;
	int num_of_var=0;
	char* get_mime;
	char method[BYTESIZE];
	char req_url[BYTESIZE];
	char version[BYTESIZE];
	char t[BYTESIZE];
	int flag=0;
	if(!request){
		flag=buildErrMsg(NULL,NULL,BADREQUEST,client_socket);
		if(flag!=SUCCESS){
			close(client_socket);
			return false;
		}
			close(client_socket);
			return true;
	}
	
	/*First I checked if the request isNULL or just the all 3 tokens*/
	bzero(method,BYTESIZE);
	bzero(req_url,BYTESIZE);
	bzero(t,BYTESIZE);
	bzero(version,BYTESIZE);
      
	req_url[0]='.';


	num_of_var = sscanf(request,"%s %s %s\r\n",method,t,version);
	if(num_of_var!=input_size-1){

		flag=buildErrMsg(NULL,NULL,BADREQUEST,client_socket);
		if(flag!=SUCCESS){
			close(client_socket);
			return false;
		}
		close(client_socket);
		return true;
	}
	request = "";
	strcat(req_url,t);
	


	/*Then we count the variables and the version*/
	if((strcmp(version,"HTTP/1.0")!=SUCCESS) && (strcmp(version,"HTTP/1.1")!=SUCCESS)){
		flag=buildErrMsg(version,NULL,BADREQUEST,client_socket);
		if(flag!=SUCCESS){
			close(client_socket);
			return false;
		}
			close(client_socket);
			return true;
	}
	/*check if method is GET*/
	if(strcmp(method,"GET")!=SUCCESS){
		flag=buildErrMsg("HTTP/1.1",NULL,NOTSUPPORTED,client_socket);
		if(flag!=SUCCESS){
			close(client_socket);
			return false;
		}
			close(client_socket);
			return true;
	}
	/*start to handle path*/
	/* return from isFile:
       -1 if premiision or not regular file
       0 if this is directory
       1 if its file
   */		
   DIR* p_dir;
	p_dir = opendir(req_url);

	flag_kind=kindOf(req_url);
	

	if(!p_dir && flag_kind==FAILURE){ 
		closedir(p_dir);
		flag=buildErrMsg(version,NULL,NOTFOUND,client_socket);
		if(flag!=SUCCESS){
			close(client_socket);
			closedir(p_dir);
			return false;
		}	
		close(client_socket);
		closedir(p_dir);
		return true;
	}

		get_mime=get_mime_type(req_url);
	 	int html_flag=kindOf("index.html");
	 /*index.html handle*/
	if(html_flag==INDEX_HTML){
	 	flag=handleFile(get_mime,"index.html",version,client_socket);
		if(flag!=SUCCESS){
			close(client_socket);
			closedir(p_dir);
			return false;
		}	
		close(client_socket);
			closedir(p_dir);
	return true;
	}
		/*No permission*/
	 if(flag_kind==NOACCESS){
		flag=buildErrMsg(version,NULL,FORBIDDEN,client_socket);	
		if(flag!=SUCCESS){
			close(client_socket);
			closedir(p_dir);
			return false;
		}	
		close(client_socket);
		closedir(p_dir);
		return true;
	 }
	get_mime=get_mime_type(req_url);
	 if(flag_kind==DIRECTORY){
	 	if(get_mime){
			if(strcmp(get_mime,"text/html")){
				flag=handleFile(get_mime,req_url,version,client_socket);
					if(flag!=SUCCESS)
					{
						closedir(p_dir);
						close(client_socket);
						return false;
					}	
			closedir(p_dir);
			close(client_socket);
			return true;
			}
		}	
		flag=handleDir(req_url,version,"200 OK",client_socket);
		if(flag!=SUCCESS){
			closedir(p_dir);
			close(client_socket);
			return false;
		}	
			closedir(p_dir);
		close(client_socket);
		return true;
	 }
	 if(flag_kind==FILE){
	 	flag=handleFile(get_mime,req_url,version,client_socket);
		if(flag!=SUCCESS){
			closedir(p_dir);
			close(client_socket);
			return false;
		}	
			closedir(p_dir);
			close(client_socket);
		return true;
	 }
	closedir(p_dir);
	close(client_socket);
	return false;
}
/*---------------------void handlebackslash()-------------*/
void handlebackslash(char *request){
	char method[BYTESIZE];
	char path[BYTESIZE];
	bzero(method,BYTESIZE);
	bzero(path,BYTESIZE);
	struct stat file_info;
	stat(request, &file_info);
	int req_size=0;
	int place=0;
	/*file and end with / */
	if(S_ISDIR(file_info.st_mode)==0){
		sscanf(request,"%s %s\r\n",method,path);
			req_size=strlen("GET ")+strlen(path)+strlen(" HTTP/1.1");
			place=req_size-strlen("HTTP/1.1")-p_size_place;
			if(request[place]=='/' && req_size!=JUSTGET)
				request[place]=' ';
   }
}
/*--------------------------HandleFile-----------------------------------*/
int handleFile(char* get_mime,char* req_url,char* version,int client_socket){
	/*init varibales*/
	struct stat stat_file;
	stat(req_url,&stat_file);
			
	
	int size_file = (int)stat_file.st_size; 
	char timebuff[TIME_SIZE];
	strftime(timebuff,sizeof(timebuff),RFC1123FMT,gmtime(&(stat_file.st_mtime)));

	
	/*Build the header*/
	char output[MAX_RES];
	bzero(output,MAX_RES);

	
	strcat(output,version);
	strcat(output," 200 OK\r\nServer: webserver/1.0\r\n");
	strcat(output,timebuff);
	strcat(output, "\r\n");
	if(get_mime){
		strcat(output,"Content-Type: ");
		strcat(output, get_mime);
		strcat(output, "\r\n");
	}
	strcat(output, "Content-Length: ");
	char length[50];
   sprintf(length, "%d", size_file);
	strcat(output, length);
	strcat(output, "\r\n");
	strcat(output,"Last-Modified: ");
	strcat(output,timebuff);
	strcat(output,"\r\nConnection: close\r\n\r\n");
	
	/*start to write the output to client socket  then open the URL*/
	if(write(client_socket,output,strlen(output))==FAILURE)
		return FAILURE;
	
	int fd=open(req_url,O_RDONLY);
	char buff[MAX_RES];
	bzero(buff,MAX_RES);
	
	/*Finaly read the file desciptor and write,every time reset the buffer*/
	int read_bytes=read(fd,buff, MAX_RES);
	while(read_bytes!=SUCCESS){
		if(read_bytes<0) 
			perorr("read");
		if(write(client_socket,buff,read_bytes)<0)
			perorr("write");
		memset(buff,0,MAX_RES);		
		read_bytes=read(fd,buff,MAX_RES);

	}
	
	close(client_socket);
	close(fd);
 	return SUCCESS;
}
/*---------------------------handle Dir--------------------------*/
int handleDir(char* path,char* version,char* kind,int client_socket){

	struct stat file_info;
	struct dirent *dp;
	DIR* pDir;
	if(stat(path, &file_info)==FAILURE)
		return FAILURE;

	pDir=opendir(path);
	dp=readdir(pDir);
	int counter=0;
	char timebuf[TIME_SIZE];
	char tmp[BYTESIZE];
	int place= 0;
	bzero(timebuf,TIME_SIZE);
	bzero(tmp,BYTESIZE);
	
	if(dp<0)
		return FAILURE;
		
	if(pDir!=NULL){
		while((dp=readdir(pDir))){
			counter++;
		}
		
	}
	char output[(MAX_REQ*counter)];
	bzero(output,(MAX_REQ*counter));
	
	
	while(place+1<sizeof(path))
	{
		tmp[place]=path[place+1];
		place++;
	}



	/*start to write the header*/
	strftime(timebuf, sizeof(timebuf), RFC1123FMT,gmtime(&(file_info.st_ctime)));
	strcat(output,version);
	strcat(output," ");
	strcat(output,kind);
	strcat(output,"\r\nServer: webserver/1.0\r\n");
	strcat(output,"Date: ");
	strcat(output,timebuf);
   strcat(output,"\r\n");
	strcat(output,"Content-Type: ");
	strcat(output,"text/html\r\n");
	strcat(output,"Content-Length: ");
	char length[50]="";
   strcat(output,length);
   strcat(output,"\r\n");
	strcat(output,"Last-Modified: ");
	strcat(output, timebuf);
	strcat(output,"\r\n");
	strcat(output,"Connection: close\r\n\r\n");

	
	/*The dir content table-HTML view*/
	strcat(output,"<HTML>\r\n<HEAD><TITLE>");
	strcat(output,"Index of ");
	strcat(output,path);
	strcat(output,"</TITLE></HEAD>");
	strcat(output,"\r\n<BODY>\r\n");
	strcat(output,"<H4>Index of ");
	strcat(output,path);
	strcat(output,"</H4>");
	strcat(output,"\r\n<table CELLSPACING=8>\r\n<tr><th>");
	strcat(output,"Name</th><th>");
	strcat(output,"Last Modified</th><th>");
	strcat(output,"Size</th></tr>\r\n");
	
	closedir(pDir);
	/*Strat work on directory*/
	pDir=opendir(path);
	if(pDir)
	{
		while((dp=readdir(pDir)))
		{
			stat(dp->d_name,&file_info);

			strcat(output,"<tr><td><A HREF=\"");

			char cwd[MAX_RES];
			getcwd(cwd, sizeof(cwd));
			strcat(cwd,tmp);
			strcat(cwd,dp->d_name);
			stat(cwd,&file_info);
			strcat(output, dp->d_name);
			if(S_ISDIR(file_info.st_mode)!=0)//Dir
				strcat(output, "/\">");
			else if(S_ISDIR(file_info.st_mode)==0)//File
				strcat(output, "\">");
		

			strcat(output,dp->d_name);
			strcat(output,"</A></td><td>");
			char timebuf2[BYTESIZE];
			char file_path[MAX_REQ];
			bzero(file_path,MAX_REQ);
			bzero(timebuf2,BYTESIZE);
			struct stat f_info;
			strcpy(file_path, path);
			strcat(file_path, dp->d_name);
			stat(file_path, &f_info);
			strftime(timebuf2, sizeof(timebuf2), RFC1123FMT, gmtime(&(file_info.st_mtime)));
			strcat(output, timebuf2);
			strcat(output,"</td><td>");
			if(S_ISDIR(f_info.st_mode)==SUCCESS){
				char buffer[MAX_REQ];
				sprintf(buffer,"%d",(int)f_info.st_size);
		    	strcat(output, buffer);
			}
			strcat(output,"</td><tr>");

 	  }  
	}
	strcat(output,"</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n</BODY></HTML>");
	if(write(client_socket,output,MAX_REQ*counter)==FAILURE)
		 return FAILURE;
	closedir(pDir);
	return SUCCESS;
	
}

/*-----------------kindOf------------------------------------*/
int kindOf(char* req_url){
	struct stat  file_info;
	
	if(stat(req_url, &file_info)<0)
		return FAILURE;
	DIR* pDir;
	pDir=opendir(req_url);
	if(!pDir){
		if(errno==EACCES)
			return NOACCESS;
	}
	
	if(S_ISDIR(file_info.st_mode)!=0){
		closedir(pDir);
		return DIRECTORY;
	}
	if(S_ISREG(file_info.st_mode)==0){
		closedir(pDir);
		return NOACCESS;
	}
	closedir(pDir);
	return FILE;
}

/*-----------------------buildErrMsg-------------------------*/
int buildErrMsg(char* version,char* path,int err_num,int client_socket){
	char* kind="";
	time_t now;
	char timebuf[TIME_SIZE];
	char output[MAX_RES];
	int size=0;
	char response[MAX_RES];
	
	bzero(response,MAX_RES);
	bzero(output,MAX_RES);

	output[0] = '\0';


	/*if I didnt get the version*/
	if(!version)
		version="HTTP/1.0";
		
	/*time handle*/
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	
	char length[BYTESIZE];

	/*err-insert the kind and the length to length finaly the support response*/
	
	if(err_num==NOTSUPPORTED){
		kind="501 Not supported";
		sprintf(length,"%d",(int)strlen(response501));
		sprintf(response,"%s",response501);
	}
	if(err_num==BADREQUEST){
		kind="400 Bad Request";
		sprintf(length,"%d",(int)strlen(response400));
		sprintf(response,"%s",response400);
	}
	if(err_num==FORBIDDEN){
		kind="Forbidden";
		sprintf(length,"%d",(int)strlen(response403));
		sprintf(response,"%s",response403);
	}
	
	if(err_num==NOTFOUND){
		kind="Not Found";
		sprintf(length,"%d",(int)strlen(response404));
		sprintf(response,"%s",response404);
	}
	
	if(err_num==INTERNAL){
		kind="Internal Server Error";
		sprintf(length,"%d",(int)strlen(response500));
		sprintf(response,"%s",response500);
	}
	
	if(err_num==FOUND){
		kind="Found";
		sprintf(length,"%d",(int)strlen(response302));
		sprintf(response,"%s",response302);
	}	
	
	/*start parsing header*/
	strcat(output,version);
	strcat(output," ");
	strcat(output,kind);
	strcat(output,"\r\nServer: webserver/1.0\r\n");
	strcat(output,"Date: ");
	strcat(output,timebuf);
	
	/*If path is inserted*/
	if(path!=NULL){
		strcat(output,"\r\n");
		strcat(output,path);
		strcat(output,"/");
		size=size+strlen(path)+1;
	}

	/*continue to parse the content length is the length then import */
	strcat(output,"\r\nContent-Type: text/html");
	strcat(output,"\r\nContent-Length: ");
	strcat(output, length);
	strcat(output,"\r\nConnection: close\r\n\r\n");	
	/*Add the HTML response*/
	strcat(output,response);
	
	if(write(client_socket,output,strlen(output))==FAILURE){
			return FAILURE;
	}
	return SUCCESS;
	
}
/*-------perror()--------*/
void perorr(char* error){
	perror(error);
	exit(FAILURE);
}
/*------------------------get_mime_type-------------------*/
char *get_mime_type(char *name)
{
	char *ext = strrchr(name, '.');
	if (!ext) 
		return NULL;
	if (strcmp(ext, ".html")==0 || strcmp(ext,".htm")==0) 
		return "text/html";
	if (strcmp(ext, ".jpg")==0 || strcmp(ext,".jpeg")==0) 
		return "image/jpeg";
	if (strcmp(ext, ".gif")==0) 
		return "image/gif";
	if (strcmp(ext, ".png")==0) 
		return "image/png";
	if (strcmp(ext, ".css")==0) 
		return "text/css";
	if (strcmp(ext, ".au")==0) 
		return "audio/basic";
	if (strcmp(ext,".wav")==0) 
		return "audio/wav";
	if (strcmp(ext,".avi")==0) 
		return "video/x-msvideo";
	if (strcmp(ext,".mpeg")==0 || strcmp(ext,".mpg")==0) 
		return "video/mpeg";
	if (strcmp(ext,".mp3")==0) 
		return "audio/mpeg";
	return NULL;
}


