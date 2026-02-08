#include "response_parse.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Case-insensitive search for needle in haystack, but only within the first len bytes of haystack
static char *strncasestr(const char *haystack, const char *needle, size_t len)
{
	size_t nlen = strlen(needle);
	for (size_t i = 0; i + nlen <= len; i++)
	{
		if (strncasecmp(haystack + i, needle, nlen) == 0)
			return (char *)(haystack + i);
	}
	return NULL;
}

// Helper function to find the first occurrence of CRLF in a buffer, returns pointer to CRLF or NULL if not found
static const char *find_crlf(const char *buf, size_t len)
{
	for (size_t i = 0; i + 1 < len; i++)
	{
		if (buf[i] == '\r' && buf[i + 1] == '\n')
			return buf + i;
	}
	return NULL;
}

// Parses the HTTP response in buf and fills the ParsedResponse struct. Returns 0 on success, -1 on failure (e.g. malformed response).
int ParsedResponse_parse(ParsedResponse *res, const char *buf, size_t buflen)
{
	memset(res, 0, sizeof(*res));
	res->content_length = -1;
	res->chunked = 0;

	/* find end of headers */
	char *end = strncasestr(buf, "\r\n\r\n", buflen);
	if (!end)
		return -1;

	res->header_len = (end - buf) + 4;

	/* parse status line */
	// const char *line_end = strstr(buf, "\r\n");
	const char *line_end = find_crlf(buf, buflen);
	if (!line_end)
		return -1;

	char status_line[256];
	size_t line_len = line_end - buf;
	if (line_len >= sizeof(status_line))
		return -1;

	memcpy(status_line, buf, line_len);
	status_line[line_len] = '\0';

	/* Example: HTTP/1.1 200 OK */
	char *saveptr;
	char *version = strtok_r(status_line, " ", &saveptr);
	char *code = strtok_r(NULL, " ", &saveptr);
	char *text = strtok_r(NULL, "", &saveptr);

	if (!version || !code)
		return -1;

	res->version = strdup(version);
	res->status_code = atoi(code);
	res->status_text = text ? strdup(text) : NULL;

	/* parse headers */
	const char *headers = line_end + 2;
	size_t headers_len = res->header_len - (headers - buf);

	char *cl = strncasestr(headers, "Content-Length:", headers_len);
	if (cl)
		res->content_length = atoi(cl + 15);

	if (strncasestr(headers, "Transfer-Encoding: chunked", headers_len))
		res->chunked = 1;

	return 0;
}

// Frees any dynamically allocated memory in the ParsedResponse struct
void ParsedResponse_destroy(ParsedResponse *res)
{
	if (res->version)
		free(res->version);
	if (res->status_text)
		free(res->status_text);
}
