
#define  _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <ctype.h>

#ifndef PROXY_PARSE
#define PROXY_PARSE


struct ParsedRequest //cấu trúc của 1 request http từ browser gửi đi
{
	//phần request line
	char *method;
	char *protocol;
	char *host;
	char *port;
	char *path;
	char *version;
	char *buf;
	size_t buflen;

	//phần header request
	struct ParsedHeader *arrHeader;
	size_t headerUsed;
	size_t headerLen;

	//phần bổ trợ phương thức POST (nếu có)
	char *post;
	size_t postlen;	
};

struct ParsedHeader //cấu trúc của 1 header
{
	char * key;
	size_t keylen;
	char * value;
	size_t valuelen;
};

//khởi tạo
struct ParsedRequest* ParsedRequest_create();

//   Phân tích gói Request nhận được từ browse
int ParsedRequest_parse(struct ParsedRequest * parse, const char *buf,int buflen);

// Từ struct thông tin được lưu thành Request
int ParsedRequest_unparse(struct ParsedRequest *pr, char *buf, size_t buflen);

//build lại header thành 1 chuỗi, lưu vào vùng nhớ buf
int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf,size_t buflen);

//trả về độ dài của full request 
size_t ParsedRequest_totalLen(struct ParsedRequest *pr);

// BUILD lại dòng Request line từ struct chứa thông tin của Request 
int ParsedRequest_printRequestLine(struct ParsedRequest *pr, char * buf, size_t buflen, size_t *tmp);

// trả về chiều dài của dài của dòng request line
size_t ParsedRequest_requestLineLen(struct ParsedRequest *pr);

//giải phóng vùng nhớ đã cấp phát cho gói request
void ParsedRequest_destroy(struct ParsedRequest *pr);

//---------hàm cho Header Request 
// Khởi tạo mảng các struct chứa các cặp header
void ParsedHeader_create(struct ParsedRequest *pr);

// Tổng độ dài phần header
size_t ParsedHeader_headerLen(struct ParsedRequest *pr);

//thêm cặp giá trị key và value vào struct 
int ParsedHeader_set(struct ParsedRequest *pr, const char * key,const char * value);

// trả về con trỏ đến cặp gia trị tương ứng nếu  có 
struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr,	const char * key);

// Xóa cặp giá trị tương ứng nếu có
int ParsedHeader_remove(struct ParsedRequest *pr, const char * key);

//	phân tích line được truyền vào thành cặp giá trị pr->arrHeader
int ParsedHeader_parse(struct ParsedRequest * pr, char * line);

// Trả về độ dài của 1 dòng header
size_t ParsedHeader_lineLen(struct ParsedHeader * ph);

//build 1 struct Request thành 1 chuỗi request 
int ParsedHeader_printHeaders(struct ParsedRequest * pr, char * buf, size_t len);

//giải phóng cặp header được truyền vào
void ParsedHeader_destroyOneHeader(struct ParsedHeader * ph);

//giải phóng vùng nhớ đã cấp phát cho struct chứa thông tin header
void ParsedHeader_destroy(struct ParsedRequest * pr);
#endif