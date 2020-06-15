#include "stdafx.h"
#include "ParseRequest.h"

#define MAX_REQ_LEN 8192
#define MIN_REQ_LEN 4

const char *slash = "/";
// Khởi tạo 
struct ParsedRequest* ParsedRequest_create()
{
	struct ParsedRequest *pr;
	pr = (struct ParsedRequest *)malloc(sizeof(struct ParsedRequest));
	if (pr != NULL)
	{
		ParsedHeader_create(pr);
		pr->buf = NULL;
		pr->method = NULL;
		pr->protocol = NULL;
		pr->host = NULL;
		pr->path = NULL;
		pr->version = NULL;
		pr->buf = NULL;
		pr->buflen = 0;
		pr->post = NULL;
		pr->postlen = 0;
	}
	return pr;
}

/*
   Phân tích gói Request nhận được từ browse
	Kết quả trả về :
   0 nếu không có lỗi xảy ra
   -1 nếu phân tích không thành công
*/
int ParsedRequest_parse(struct ParsedRequest * parse, const char *buf, int buflen)
{
	// KHAI BAO 
	char *full_addr; // Địa chỉ URI
	char *saveptr;	// con trỏ cho vùng nhớ tạm
	char *index;	// Con trỏ lưu vị trí của vùng nhớ tạm
	char *currentHeader;	// địa chỉ vùng nhớ tạm cho từng cặp giá trị Header


	// KIEM TRA Request Line
	if (parse->buf != NULL)
	{
		return -1;
	}
	if (buflen < MIN_REQ_LEN || buflen > MAX_REQ_LEN)
	{
		return -1;
	}
	//------------------------------------------------------------------------------------------


		// Copy Request vào 1 vùng nhớ tạm để xử lý
	char *tmp_buf = (char *)malloc(buflen + 1);
	memcpy(tmp_buf, buf, buflen);
	tmp_buf[buflen] = '\0';


	index = strstr(tmp_buf, "\r\n\r\n"); // Lấy vị trí kết thúc của phần Header HTTP Request
	if (index == NULL)
	{
		free(tmp_buf);
		return -1;
	}

	// Lấy vị trí kết thúc của Request Line
	index = strstr(tmp_buf, "\r\n");
	if (parse->buf == NULL)
	{
		parse->buf = (char *)malloc((index - tmp_buf) + 1); // Cấp phát động cho parse->buf để lưu requset Line
		parse->buflen = (index - tmp_buf) + 1;
	}
	memcpy(parse->buf, tmp_buf, index - tmp_buf); // Copy Request Line từ vùng nhớ tạm vào Parse->Buf
	parse->buf[index - tmp_buf] = '\0';

	// Phân tích cấu trúc của Request Line Lưu vào Struct 
	parse->method = strtok_s(parse->buf, " ", &saveptr); // tách phần Method từ Request Line
	if (parse->method == NULL)
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}
	// Kiểm tra Method
	if (strcmp(parse->method, "GET") && strcmp(parse->method, "POST"))
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}
	// Tách địa chỉ cần truy cập
	full_addr = strtok_s(NULL, " ", &saveptr);
	if (full_addr == NULL)
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}
	// Version của HTTP 
	parse->version = full_addr + strlen(full_addr) + 1;
	if (parse->version == NULL)
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}
	// Kiểm tra có phải là phương thức HTTP hay không
	if (strncmp(parse->version, "HTTP/", 5))
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}

	// Tách phần Protocol từ phần địa chỉ web truy cập 
	parse->protocol = strtok_s(full_addr, "://", &saveptr);
	if (parse->protocol == NULL)
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}

	// Lưu URI
	const char *rem = full_addr + strlen(parse->protocol) + strlen("://");
	size_t abs_uri_len = strlen(rem);

	// Tách  lấy Phần Host 
	parse->host = strtok_s(NULL, "/", &saveptr);
	if (parse->host == NULL)
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}

	if (strlen(parse->host) == abs_uri_len)
	{
		free(tmp_buf);
		free(parse->buf);
		parse->buf = NULL;
		return -1;
	}


	parse->path = strtok_s(NULL, " ", &saveptr);
	if (parse->path == NULL)
	{    // Nếu phần path không có trong phần địa chỉ thì thay phần path thành kí tự '/'
		int rlen = strlen(slash);
		parse->path = (char *)malloc(rlen + 1);
		strncpy(parse->path, slash, rlen + 1);
	}
	else
		if (strncmp(parse->path, slash, strlen(slash)) == 0) // trường hợp path "//"
		{
			free(tmp_buf);
			free(parse->buf);
			parse->buf = NULL;
			parse->path = NULL;
			return -1;
		}
		else
		{
			// copy parse->path, prefix with a slash
			char *tmp_path = parse->path;
			int rlen = strlen(slash);
			int plen = strlen(parse->path);
			parse->path = (char *)malloc(rlen + plen + 1);
			strncpy(parse->path, slash, rlen);
			strncpy(parse->path + rlen, tmp_path, plen + 1);
		}
	// Tách lấy host bỏ phần port
	parse->host = strtok_s(parse->host, ":", &saveptr);
	//Tách lấy phần port
	parse->port = strtok_s(NULL, "/", &saveptr);

	if (parse->host == NULL)
	{
		free(tmp_buf);
		free(parse->buf);
		free(parse->path);
		parse->buf = NULL;
		parse->path = NULL;
		return -1;
	}

	if (parse->port != NULL)
	{
		// Nếu có tồn tại port trong Request Line
		int port = strtol(parse->port, (char **)NULL, 10);// đổi kí tự từ chuỗi thành số

	}
	// KET THUC PHAN PHAN TICH REQUEST LINE 
	//----------------------------------------------------------------------------------------------------------------------------

		// Phân tích các mục Header
	int ret = 0;
	currentHeader = strstr(tmp_buf, "\r\n") + 2;
	while (currentHeader[0] != '\0' && !(currentHeader[0] == '\r' && currentHeader[1] == '\n'))
	{
		if (ParsedHeader_parse(parse, currentHeader))
		{
			//Phân tích thất bại => dừng phân tích request
			ret = -1;// Ket qua sai
			break;
		}

		currentHeader = strstr(currentHeader, "\r\n"); // tìm vị trí kết thúc của dòng
		if (currentHeader == NULL || strlen(currentHeader) < 2)
			break;
		// Phân tích dòng tiếp theo 
		currentHeader += 2;
	}
	// Tìm vị trí của Phần Body Request (Nội dung của method Post) nếu có
	index = strstr(tmp_buf, "\r\n\r\n");
	if (index)
	{
		// nếu có thì lưu vào phần post
		parse->postlen = strlen(tmp_buf) - (index - tmp_buf) + 1;
		parse->post = (char *)malloc(parse->postlen);
		memcpy(parse->post, index + 4, parse->postlen);
	}


	free(tmp_buf);
	return ret;
}

//build lại header thành 1 chuỗi, lưu vào vùng nhớ buf
int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, size_t buflen)
{
	if (!pr || !pr->buf)
		return -1;

	if (ParsedHeader_printHeaders(pr, buf, buflen) < 0)
		return -1;
	return 0;
}

//giải phóng vùng nhớ đã cấp phát cho gói request
void ParsedRequest_destroy(struct ParsedRequest *pr)
{
	//giải phóng request line
	if (pr->buf != NULL)
	{
		free(pr->buf);
	}

	if (pr->path != NULL) {
		free(pr->path);
	}
	//giải phóng toàn bộ các cặp header
	if (pr->headerLen > 0)
	{
		ParsedHeader_destroy(pr);
	}
	free(pr);
}

// Khởi tạo mảng các struct chứa các cặp header
void ParsedHeader_create(struct ParsedRequest *pr)
{
	pr->arrHeader = (struct ParsedHeader *)malloc(sizeof(struct ParsedHeader)*8);
	pr->headerLen = 8;
	pr->headerUsed = 0;
}

// Tổng độ dài phần header
size_t ParsedHeader_headerLen(struct ParsedRequest *pr)
{
	if (!pr || !pr->buf)
		return 0;

	size_t i = 0;
	int len = 0;
	while (pr->headerUsed > i)
	{
		len += ParsedHeader_lineLen(pr->arrHeader + i);
		i++;
	}
	len += 2;// 2 kí tự "\r\n"
	return len;
}

//thêm cặp giá trị key và value vào struct 
int ParsedHeader_set(struct ParsedRequest *pr,const char * key, const char * value)
{
	struct ParsedHeader *header;
	ParsedHeader_remove(pr, key); // tìm và hủy nếu đã tồn tại header key trước đó

	// Kiểm tra số cặp tối đa có đủ để đó
	if (pr->headerLen <= pr->headerUsed + 1)
	{
		pr->headerLen = pr->headerLen * 2;
		pr->arrHeader=(struct ParsedHeader *)realloc(pr->arrHeader,pr->headerLen * sizeof(struct ParsedHeader));
		if (!pr->arrHeader)
			return -1;

	}

	header = pr->arrHeader + pr->headerUsed; // Trỏ đến phần tử tiếp theo trong mảng Header trong struct
	pr->headerUsed += 1; // 
	// gán giá trị cho key và value cho arrheader[headerUsed]
	header->key = (char *)malloc(strlen(key) + 1);
	memcpy(header->key, key, strlen(key));
	header->key[strlen(key)] = '\0';
	header->value = (char *)malloc(strlen(value) + 1);
	memcpy(header->value, value, strlen(value));
	header->value[strlen(value)] = '\0';
	
	header->keylen = strlen(key) + 1;
	header->valuelen = strlen(value) + 1;
	return 0;
}

// trả về con trỏ đến cặp gia trị tương ứng nếu  có 
struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr,	const char * key)
{
	size_t i = 0;
	struct ParsedHeader * tmp;
	while (pr->headerUsed > i)
	{
		// Kiểm tra từng cặp header 
		tmp = pr->arrHeader + i;
		if (tmp->key && key && strcmp(tmp->key, key) == 0)// nếu tồn tại thì return con trỏ đến cặp giá trị đó
		{
			return tmp;
		}
		i++;
	}
	return NULL;
}

// Xóa cặp giá trị tương ứng nếu có
int ParsedHeader_remove(struct ParsedRequest *pr, const char *key)
{
	struct ParsedHeader *tmp;
	tmp = ParsedHeader_get(pr, key);// tìm xem đã tồn tại cặp header có giá trị key hay chưa
	if (tmp == NULL) // nếu không tồn tại
		return -1;
	// nếu tồn tại thì hủy giá trị đó đi 
	free(tmp->key);
	free(tmp->value);
	tmp->key = NULL;
	return 0;
}

/*
	phân tích line được truyền vào thành cặp giá trị pr->arrHeader
	trả về 0 nếu thành công
		-1 nếu thất bại

*/
int ParsedHeader_parse(struct ParsedRequest * pr, char * line)
{
	char * key;
	char * value;
	char * index1;
	char * index2;

	index1 = strstr(line, ":");// tìm vị trí kết thúc của Header Key
	if (index1 == NULL)
	{
		return -1;
	}
	// cấp phát bộ nhớ và lấy giá trị cho header key trong dòng header line được truyền vào
	key = (char *)malloc((index1 - line + 1) * sizeof(char));
	memcpy(key, line, index1 - line);
	key[index1 - line] = '\0';

	index1 += 2; // vị trí bắt đầu của Value tương ứng với header key
	index2 = strstr(index1, "\r\n"); // vị trí kết thúc dòng hiện tại

	// cấp phát bộ nhớ và lấy giá trị cho value trong dòng header line được truyền vào                  
	value = (char *)malloc((index2 - index1 + 1) * sizeof(char));
	memcpy(value, index1, (index2 - index1));
	value[index2 - index1] = '\0';

	// Lưu cặp giá trị vừa lấy được vào struct
	ParsedHeader_set(pr, key, value);
	free(key);
	free(value);
	return 0;
}

// Trả về độ dài của 1 dòng header
size_t ParsedHeader_lineLen(struct ParsedHeader * ph)
{
	if (ph->key != NULL)
	{
		return strlen(ph->key) + strlen(ph->value) + 4;
	}
	return 0;
}

//build 1 struct Request thành 1 chuỗi request 
int ParsedHeader_printHeaders(struct ParsedRequest * pr, char * buf,size_t len)
{
	char * current = buf;
	struct ParsedHeader * header;
	size_t i = 0;

	if (len < ParsedHeader_headerLen(pr))
	{
		// Nếu không đủ bộ nhớ để lưu các giá trị header
		return -1;
	}

	//duyệt các cặp header, copy các giá trị và thêm ký tự cho đúng cú pháp gói resquest
	while (i < pr->headerUsed)
	{
		header = pr->arrHeader + i;	//lấy header thứ i
		if (header->key) 
		{
			memcpy(current, header->key, strlen(header->key));
			memcpy(current + strlen(header->key), ": ", 2);
			memcpy(current + strlen(header->key) + 2, header->value,strlen(header->value));
			memcpy(current + strlen(header->key) + 2 + strlen(header->value),"\r\n", 2);
			current += strlen(header->key) + strlen(header->value) + 4;
		}
		i++;
	}
	memcpy(current, "\r\n", 2);
	return 0;
}

//giải phóng cặp header được truyền vào
void ParsedHeader_destroyOneHeader(struct ParsedHeader * ph)
{
	if (ph->key != NULL)
	{
		free(ph->key);
		ph->key = NULL;
		free(ph->value);
		ph->value = NULL;
		ph->keylen = 0;
		ph->valuelen = 0;
	}
}

//giải phóng vùng nhớ đã cấp phát cho struct chứa thông tin header
void ParsedHeader_destroy(struct ParsedRequest * pr)
{
	size_t i = 0;
	//giải phóng từng cặp header
	while (i < pr->headerUsed)
	{
		ParsedHeader_destroyOneHeader(pr->arrHeader + i);
		i++;
	}
	pr->headerUsed = 0;
	//giải phóng mảng các header
	free(pr->arrHeader);
	pr->headerLen = 0;
}











