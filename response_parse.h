#ifndef RESPONSE_PARSE_H
#define RESPONSE_PARSE_H

#include <stddef.h>

typedef struct ParsedResponse
{
	char *version;		// "HTTP/1.0" or "HTTP/1.1"
	int status_code;	// 200, 404, etc
	char *status_text;	// "OK", "Not Found" (optional)
	int content_length; // -1 if not present
	int chunked;		// 1 if Transfer-Encoding: chunked
	size_t header_len;	// bytes until \r\n\r\n
} ParsedResponse;

/* Parse only the RESPONSE HEADERS */
int ParsedResponse_parse(ParsedResponse *res,
						 const char *buf,
						 size_t buflen);

/* Cleanup */
void ParsedResponse_destroy(ParsedResponse *res);

#endif
