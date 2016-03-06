-----------------------------------------------
roich
Roi Cohen 301063814
Ex4- HTTP Server

Files:
	1)readme
	2)server.c
-----------------------------------------------	
NOTE:inside folsers I cant see the size of the files.
		noaccess didnt work for files just folders
-----------------------------------------------	

Exercise Description:
--------------------------------
bool isNumber(char*);
	function that get the arg and check if this char is number.
	this is a while loop that check all the args is between 0-9.

--------------------------------
bool checkInput(char**,int);
	Function that get the argv and the argc checks if the there is 4 arguments(with the ./app).
	Calls to isNumber for checks if it number or char-->true/false.

--------------------------------
void connectsocket(int,int,int);
	Function that get 3 insts:the port,number of request and the pool size.
	Strat to connect to socket by the algorithm:
	socket->bind->listen->create the pool->accept.
	After create the threadpool for loop that create the file desciptor of the socket,
	than for all of them do accept than call to-dispathch here.

--------------------------------
int dispatch_here(void*);
	function that get the client socket(void*) becuase I did malloc to socket and return int:
	-1 if dosn't  succedd and 0 if succed->if return -1 that build errMsg with 500 INTERNAL err.
	First I do a while loop for reading the request from socket to request and if it dosnt succed
	build err msg, end_request to now how much I read.
	Than call to handlebackslash method,if the request url have / in the end and this is file so
	cut it out.
	After it call to setRequest function to handle the request if return -1 than return 500 		    Internal err.
	Finally free the request and return 0.
--------------------------------
bool setRequest(char*,int);
	Function that handle the request thats mean that if I got version that is isn't 
	HTTP/1.0 or HTTO/1.1 call buildErrMsg also when I got no GET return NOTSUPPORTED,
	also if the request is NULL so there is no request-->BADREQUEST.
	After finish all checks check if there is any index.html file if there is 
	call handlFile method and cloase the socket and the opendir.
	After it check the ACCESS with errno inside kindOf method that return if its
	File/Dir/NOACCESS.
	

--------------------------------
void handlebackslash(char *request);
	Function that get the request as it and check if the path is file 
	check if there is any / in the if there is so cut it!
	
--------------------------------
int handleFile(char*,char*,char*,int);
	Function that get the type of the file,the path of the request the version and the socket
	Then start to write 200 OK than write it to the socket and open this request.
	When finish start to read the request to the buffer.
	If something goes wrong return FAILURE to setRequest and return INTERNAL
	if it succedd return 0 
	
--------------------------------
int handleDir(char*,char*,char*,int);
		Function that get the path of the request the version, the kind  and the socket
		while loop to count how much files there inside the directory.
		Than start to write the header to output varibale, after it strat to write 
		the HTML table, when finish close the open dir and reopen it and start
		to read the dir,,I used cwd to get all the path and than check if its dir
		or file->if dir so put in the end /.
		In the end close the table HTML and write this outpur to the socket
		return 0 if all goes well.
		
--------------------------------
int kindOf(char*);
	Function that get the path and checks what is it, Directory,File,File with no access
	by usinf S_ISDIR if its !=0 so its dir o.w S_ISREG==0 so there is no access.

	
---------------------------------
int buildErrMsg(char*,char*,int,int);
	Function that get the version ,the path,err number, and the socket.
	if its FORBIDDEN or something else put all reqpose that match to issue isinde response.
	than put all the rest of the header.
	than checks the path for knowing if its insnt NULL put it inside the output.
	Finally write it to the socket.
	
--------------------------------------------------
void perorr(char*);
 My perror  function to print the current err and exit with -1
	
--------------------------------------------------


