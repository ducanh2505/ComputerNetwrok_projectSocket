// Demo_Server.cpp : Defines the entry point for the console application.
//
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Resource.h"
#include "stdafx.h"
#include "afxsock.h"
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <queue>
#include "ParseRequest.h"
#pragma comment(lib, "Ws2_32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define MAX_CACHE 500	//cache tối đa 500 request
using namespace std;

vector<string> blacklisted;//mảng chứa các domain bị cấm
char* ResForbidden ="HTTP/1.0 403 Forbidden\r\n\Cache-Control: no-cache\r\n\Connection: close\r\n";

unordered_map<string, string> main_cache;		//bộ nhớ chính lưu giữ các gói response đã được cache	
queue<string> order_request;		// thứ tự các gói request được gửi

CWinApp theApp;
DWORD threadID;
HANDLE threadStatus;

int LoadBlackList(vector<string>& arr)
{
	fstream f;
	f.open("blacklist.conf", ios::in | ios::out);
	if (f.is_open())
	{
		while (!f.eof()) {
			string temp;
			getline(f, temp);
			if (temp.compare(0, 4, "www.") == 0)
				temp = temp.substr(4);
			if (temp.back() == '\n')
			{
				temp.pop_back();
			}
			arr.push_back(temp);
		}
	}
	else { return -1; }
	return 0;
}

bool CheckDomain(string hostname)
{
	if (hostname.compare(0, 4, "www.") == 0)
		hostname = hostname.substr(4);	//del "wwww." nếu có
	if (blacklisted.size() > 0)
	{
		for (auto i : blacklisted)
		{
			string tmp = i;
			if (tmp == hostname)
				return 1;
		}
	}
	return 0;
}

char* convert_Request_to_String(struct ParsedRequest *req)
{

	// Thiết lập trường giá trị cho header
	// Host và conection thành close
	//ParsedHeader_set(req, "Host", req->host);
	//ParsedHeader_set(req, "Connection", "close");

	int iheaderLen = ParsedHeader_headerLen(req);// tổng chiều dài của header
	//vùng nhớ chứa chuỗi lưu Header
	char *headersBuf;
	headersBuf = (char*)malloc(iheaderLen + 1);

	if (headersBuf == NULL)
	{
		return NULL;
	}
	//build lại phần header request với 2 header vừa thêm ở đầu hàm
	ParsedRequest_unparse_headers(req, headersBuf, iheaderLen);
	headersBuf[iheaderLen] = '\0';

	//kích thước mới của reqưquest
	int request_size = strlen(req->method) + strlen(req->path) + strlen(req->version) + iheaderLen + 4+strlen(req->post)+2;

	//kết quả full gói request cần trả về
	char *serverReq;
	serverReq = (char *)malloc(request_size + 1);

	if (serverReq == NULL)//lỗi cấp phát 
	{
		return NULL;
	}

	//nối lại từng phần trong struct req vào chuỗi
	serverReq[0] = '\0';
	strcpy(serverReq, req->method);
	strcat(serverReq, " ");
	strcat(serverReq, req->path);
	strcat(serverReq, " ");
	strcat(serverReq, req->version);
	strcat(serverReq, "\r\n");
	strcat(serverReq, headersBuf);
	strcat(serverReq, req->post);
	free(headersBuf);

	return serverReq;
}
string  getReqLine(struct ParsedRequest *req)
{
	string ret="";
	ret += string(req->method);
	ret += " ";
	ret += string(req->host);
	ret += string(req->path);
	ret += " ";
	ret += string(req->version);
	ret += "\r\n";
	return ret;
}

int createServerSocket(char *Host, char *Port)
{

	struct addrinfo ahints;
	struct addrinfo *Result;

	int iSockfd;

	/* Get address information for stream socket on input port */
	//
	memset(&ahints, 0, sizeof(ahints));
	ahints.ai_family = AF_UNSPEC;  //  mặc định cho cả địa chỉ ipv4 và ipv6 
	ahints.ai_socktype = SOCK_STREAM; // kiểu socket cho kết nối TCP
	if (getaddrinfo(Host, Port, &ahints, &Result) != 0)
	{
		return -1;
	}

	// tạo socket với thông tin của WebServer 
	if ((iSockfd = socket(Result->ai_family, Result->ai_socktype, Result->ai_protocol)) < 0) 
	{
		return -1;
	}

	// Kết nối socket với Web Server
	if (connect(iSockfd, Result->ai_addr, Result->ai_addrlen) < 0) 
	{
		fprintf(stderr, " Error in connecting to server %s ! \n", Host);
		return -1;
	}

	freeaddrinfo(Result);
	// trả về Socket đã kết nối được với Web Server
	return iSockfd;
}

int writeToWebServer(const char* buff_to_server, int sockfd, int buff_length)
{

	int totalsent = 0;
	int sentRet;
	// Gửi request lên web server
	while (totalsent < buff_length)
	{
		if ((sentRet = send(sockfd, (char *)(buff_to_server + totalsent), buff_length - totalsent, 0)) < 0) 
		{
			return -1;
		}
		totalsent += sentRet;

	}
	return 0;
}

int writeToClientSocket(const char* buff_to_server, int sockfd, int buff_length)
{
	int totalsent = 0;
	int SentRet = 0;

	while (totalsent < buff_length) 
	{		
		 // send char *buf cho browser
		SentRet = send(sockfd, (char *)(buff_to_server + totalsent), buff_length - totalsent, 0);
		
		if (SentRet < 0) 
		{
			return 0;
		}
		totalsent += SentRet ;
	}

	return 1;
}

string Proxy_Receive_Send(int Clientfd, int Serverfd)	
{
	int MAX_BUF_SIZE = 5000; // Kích thước bộ nhớ tạm lưu response nhận được
	int ReceiRet;			// số byte nhận được sau mỗi lần gọi hàm recv
	char buf[5000];			//	Vùng nhớ tạm chứa response
	string res;

	int kt = 1;				// biến kiểm tra quá trình nhận response từ Web Server
	
	while (kt) 
	{
		//hàm nhận response từ Web Server
		ReceiRet = recv(Serverfd, buf, MAX_BUF_SIZE, 0);

		if (ReceiRet < 0) 
		{
			return "";
		}
		else
			if (ReceiRet == 0)
				kt = 0;
			else
			{					
				//  Gửi response cho client(Browse)
				writeToClientSocket(buf, Clientfd, ReceiRet);

				// Nối từng phần của gói response lại
				res.append(buf, ReceiRet);

				// reset bộ nhớ tạm
				memset(buf, 0, 5000);	
			}
	}

	return res;	// trả về gói response nhận được
}

DWORD WINAPI processRequest(LPVOID arg)
{
	//tạo socket kết nối proxy với Client(Browse)
	SOCKET* hConnected = (SOCKET*)arg;
	//chuỗi tạm chứa request 
	int MAX_BUFFER_SIZE = 5000;
	char buf[5000];

	// chuỗi chứa request sau cùng
	char *request_message; 
	request_message = (char*)malloc(MAX_BUFFER_SIZE);

	if (request_message == NULL) 
	{
 		return -1;
	}

	request_message[0] = '\0';
	int total_received_bits = 0;
	while (strstr(request_message, "\r\n\r\n") == NULL) //nhận đến khi hết request
	{
		//nhận request từ browser lên
		int recvd = recv(*hConnected, buf, MAX_BUFFER_SIZE, 0);

		if (recvd < 0)	//lỗi khi nhận request  
		{
			return -1;
		}
		else 
			if (recvd == 0) break; //không nhận được gì từ browser nữa => ngừng nhận
			else
			{
				total_received_bits += recvd;
				//nếu tổng nhận > kích thước cấp phát ban đầu => cấp phát lại
				buf[recvd] = '\0';
				if (total_received_bits > MAX_BUFFER_SIZE)
				{

					MAX_BUFFER_SIZE *= 2;
					request_message = (char *)realloc(request_message, MAX_BUFFER_SIZE);
					if (request_message == NULL)//cấp phát không thành công
					{
						return -1;
					}
				}
			}
		strcat(request_message, buf);//chuyển request từ chuỗi tạm thành chuỗi chính
	}
	//dạng cấu trúc của request 
	struct ParsedRequest *req;  
	req = ParsedRequest_create();

	//phân tích request_message nhận được ở trên thành dạng cấu trúc
	if (ParsedRequest_parse(req, request_message, strlen(request_message)) < 0) 
		return -1;

	string hostname(req->host);
	if (CheckDomain(hostname))
	{
		writeToClientSocket(ResForbidden,*hConnected, strlen(ResForbidden));
		printf("%s IS FORBIDEN!!!\n\n", hostname.data());
	}
	else
	{
		//nếu thuộc tính port rỗng => lấy port mặc định là 80
		if (req->port == NULL)
			req->port = (char*) "80";

		//build lại gói request để gửi lên webserver
		char* browser_req = convert_Request_to_String(req);
		printf("HTTP Request line: %s\n", getReqLine(req).data());
		if (strcmp(req->method, "POST") == 0)//nếu là method post => in thêm phần post
			printf("%s\n", req->post);

		//----------------bat dau co che cache-------------------------------------
		string check_cache(browser_req);	//lấy chuỗi request

		if (main_cache.find(check_cache) != main_cache.end())	//check xem có trong cache chưa
		{
			string tmp = main_cache[check_cache];
			writeToClientSocket(tmp.data(), *hConnected, tmp.size());  // gửi về cho browser, check cái biến này nào 
		}
		else
		{
			//trường hợp req lần đầu đc gửi, chưa có trong cache
			// tạo socket tương ứng với địa chỉ của webserver
			int iServerConnect = createServerSocket(req->host, req->port);
			//gửi request lên webserver, gửi thành công => trả về 1
			int check = writeToWebServer(browser_req, iServerConnect, total_received_bits);
			string response_received;//chuỗi chứa response trả về phục vụ cho cache		
			if (check==0)//nếu gửi thành công request lên webserver => nhận gói response về
				response_received = Proxy_Receive_Send(*hConnected, iServerConnect);

			//nếu phương thức là GET => ta thực hiện cache lại gói response, nếu là POST => bỏ qua
			if (!strcmp(req->method, "GET"))
			{
				//kiểm tra queue đã đầy chưa
				if (order_request.size() == MAX_CACHE)//số gói cache được = max_cache
				{
					string first_req = order_request.front();	//lấy gói request được gửi xa nhất thời điểm đang xét
					order_request.pop();		//bỏ gói ra khỏi hàng đợi
					main_cache.erase(first_req);	//xóa khỏi bộ nhớ cache
				}

				order_request.push(check_cache);	//thêm gói vừa nhận được vào hàng đợi thứ tự
				main_cache[check_cache] = (response_received);	//lưu vào bộ nhớ cache
			}

			//đóng cổng webserver
			closesocket(iServerConnect);
		}
	}
	//giải phóng bộ nhớ đã cấp cho cấu trúc
	ParsedRequest_destroy(req);
	//đóng cổng client
	closesocket(*hConnected);
	delete hConnected;
	return 0;
}

int main(int argc, char* argv[])
{
	int nRetCode = 0;	// biến giữ kết quả khởi tạo MFC

	// trả về 1 handle cho tiến trình đang chạy tạo bởi
	// file thực thi (.exe) của chương trình này
	HMODULE hModule = ::GetModuleHandle(NULL);	

	if (hModule != NULL)
	{
		// khoi tao MFC va bao loi neu co
		if (!AfxWinInit(hModule, NULL, ::GetCommandLine(), 0))
		{
			_tprintf(_T("Error : MFC initialization failed\n"));
			system("pause");
			nRetCode = -1;
		}
		else
		{
			// Khoi tao giao dien Windows Socket
			AfxSocketInit(NULL); 

			// sockProxy : socket để nghe request từ browser
			// sockClient: được tạo mỗi khi có Request từ browser
			SOCKET *sockClient, sockProxy; 
			sockaddr_in serv_addr;	// thông tin về địa chỉ của proxy server
			sockaddr cli_addr;		// thông tin về địa chỉ của client (browser)

			int iResult;
			WSADATA wsaData;
			// Initialize Winsock
			iResult = WSAStartup(MAKEWORD(2, 2), &wsaData); // sử dụng thư viện động WS2_32.dll để check lỗi
			if (iResult != 0)
			{
				fprintf(stderr, "WSAStartup failed: %d\n", iResult);
				system("pause");
				return -1;
			}

			//hints: thông tin của socket ở phương thức http - "80"
			struct addrinfo *result = NULL, *ptr = NULL, hints;

			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			hints.ai_flags = AI_PASSIVE;
			iResult = getaddrinfo(NULL, "80", &hints, &result);

			if (iResult != 0) {
				printf("getaddrinfo failed: %d\n", iResult);
				WSACleanup();
				system("pause");
				return -1;
			}

			//tạo socket để lắng nghe ở proxy server 
			sockProxy = socket(result->ai_family, result->ai_socktype,result->ai_protocol); // create a socket 
			if (sockProxy < 0)
			{
				fprintf(stderr, "SORRY! Cannot create a socket ! \n");
				system("pause");
				return -1;
			}

			memset(&serv_addr, 0, sizeof(serv_addr));

			int portno = 8888;
			serv_addr.sin_family = AF_INET;     // ip4 family 
			serv_addr.sin_addr.s_addr = INADDR_ANY;  // represents for localhost i.e 127.0.0.1 
			serv_addr.sin_port = htons(portno);

			//liên kết địa chỉ của proxy server với sockProxy
			int binded = bind(sockProxy, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
			if (binded < 0) 
			{
				fprintf(stderr, "Error on binding! %d\n", WSAGetLastError());
				closesocket(sockProxy);
				WSACleanup();
				system("pause");
				return -1;
			}

			int clilen = sizeof(struct sockaddr);
			//proxy nhận tối đa 100 request cùng 1 lúc
			if (listen(sockProxy, 100) != 0)
			{
				fprintf(stderr, "Error on listening to client! \n\n");
				WSACleanup();
				system("pause");
				return -1;
			}

			//load blacklist từ file lên bộ nhớ để tránh việc đọc ghi file nhiều lần 
			if (LoadBlackList(blacklisted) < 0)
			{
				printf("ERROR: BLACKLIST LOADED FAIL!!!\n\n");
			}
			
			main_cache.reserve(MAX_CACHE);	//tối đa có MAX_CACHE cặp request-response
			main_cache.max_load_factor(0.25);	//khả năng đụng độ tối đa là 0,25
			printf("-------> Server is listening\n");
			do {
				sockClient = new SOCKET;
				//Bắt đầu nhận request
				
				//proxy chấp nhận kết nối của Browser
				*sockClient = accept(sockProxy, &cli_addr, (socklen_t*)&clilen);

				//Khởi tạo thread mới tương ứng với mỗi request được Proxy Server chấp nhận
				//Như vậy mỗi request sẽ được xử lý bởi một thread độc lập, không phải chờ đợi tuần tự
				threadStatus = CreateThread(NULL, 0, processRequest, sockClient, 0, &threadID);
			} while (1);
		}
	}
	else
	{
		_tprintf(_T("Fatal Error: GetModuleHandle failed\n"));//how can we knew that lỗi
		nRetCode = -1;
	}

	return nRetCode;
}