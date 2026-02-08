#include "parse.h"
#include "response_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdbool.h>


// #define MAX_CLIENTS 10						// Maximum number of clients that can be handled concurrently, not used in thread pool implementation
#define MAX_BYTES 4096						// Maximum number of bytes to read from the remote server at a time
#define MAX_ELEMENT_SIZE 10 * (1 << 10)		// 10 KB,
#define MAX_SIZE 200 * (1 << 20)			// 200 MB
#define THREAD_POOL_SIZE 8					// Number of worker threads in the thread pool
#define N_BUCKETS 64 						// Must be a power of 2 for bitmasking in hash function
#define LISTEN_BACKLOG 128					// Maximum number of pending connections in the listen queue, set to a high value to allow for more concurrent connections without rejecting them at the socket level, actual concurrency is limited by the thread pool size and job queue implementation
#define JOB_QUEUE_MAX 32					// Maximum number of jobs in the job queue, not used in current implementation but can be used to limit the number of pending jobs in the queue


// -------------------JOB-------------------------
typedef struct job{
	int client_fd;
	struct job *next;
} job_t;
// -------------------JOB QUEUE-------------------
typedef struct{
	job_t *front;
	job_t *rear;
	int size;
	int max_size;
	pthread_mutex_t mutex;
	pthread_cond_t not_empty;
} job_queue_t;
// -------------------LRU-------------------------
typedef struct cache_element
{
	char *data;
	int len;
	char *url;
	cache_element *next;
	time_t lru_time_track;
} cache_element;
// -----------------CACHE BUCKET------------------
typedef struct{
	cache_element *head;
	size_t cache_size;
	pthread_rwlock_t lock;
} cache_bucket_t;
// -----------------CACHE RESULT------------------
typedef struct
{
	char *data;
	int len;
} cache_result_t;

// -----------------------------------------------
job_queue_t job_queue;
void job_queue_init(job_queue_t *q, int max_size);
int enqueue_job(job_queue_t *q, int client_fd);
int dequeue_job(job_queue_t *q);

cache_bucket_t cache_buckets[N_BUCKETS];
cache_result_t *find_cache_element(char* );
int add_cache_element(char *, int , char *);
void remove_cache_element(cache_bucket_t *);

int PORT = 8080;
int proxy_socketId;

pthread_t workers[THREAD_POOL_SIZE];


// -------------------FUNCTIONS-------------------

int checkHTTPversion(char *msg)
{
	int version = -1;
	if (strncmp(msg, "HTTP/1.0", 8) == 0)
	{
		version = 1;
	}
	else if (strncmp(msg, "HTTP/1.1", 8) == 0)
	{
		version = 1;
	}
	else
		version = -1;

	return version;
}

int sendErrorMessage(int socket, int status_code)
{
	char str[1024];
	char currentTime[50];
	time_t now = time(0);

	struct tm data = *gmtime(&now);
	strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

	switch (status_code)
	{
	case 400:
		snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
		printf("400 Bad Request\n");
		send(socket, str, strlen(str), 0);
		break;

	case 403:
		snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
		printf("403 Forbidden\n");
		send(socket, str, strlen(str), 0);
		break;

	case 404:
		snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
		printf("404 Not Found\n");
		send(socket, str, strlen(str), 0);
		break;

	case 500:
		snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
		// printf("500 Internal Server Error\n");
		send(socket, str, strlen(str), 0);
		break;

	case 501:
		snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
		printf("501 Not Implemented\n");
		send(socket, str, strlen(str), 0);
		break;

	case 505:
		snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
		printf("505 HTTP Version Not Supported\n");
		send(socket, str, strlen(str), 0);
		break;

	default:
		return -1;
	}
	return 1;
}

int connectRemoteServer(char *host, int port)
{
	int sockfd;
	struct addrinfo hints, *res, *p;
	char port_str[16];

	snprintf(port_str, sizeof(port_str), "%d", port);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(host, port_str, &hints, &res);
	if (status != 0)
	{
		fprintf(stderr, "getaddrinfo(%s): %s\n",
				host, gai_strerror(status));
		return -1;
	}

	for (p = res; p != NULL; p = p->ai_next)
	{
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0)
			continue;

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
			break; // success

		close(sockfd);
	}

	freeaddrinfo(res);

	if (p == NULL)
	{
		fprintf(stderr, "Error: could not connect to %s:%d\n",
				host, port);
		return -1;
	}

	return sockfd;
}

int read_response_headers(int fd, char *buf, int max)
{
	int total = 0;

	while (total < max)
	{
		int n = recv(fd, buf + total, 1, 0);
		if (n <= 0)
			return -1;

		total += n;

		if (total >= 4 && strstr(buf, "\r\n\r\n") != NULL){
			printf("Finished headers (%d bytes)\n", total);
			return total;
		}
	}
	return -1;
}

int forward_chunked(int server_fd, int client_fd)
{
	char buf[MAX_BYTES];
	int n;
	int seen_final_chunk = 0;

	while ((n = recv(server_fd, buf, sizeof(buf), 0)) > 0)
	{
		send(client_fd, buf, n, 0);

		/* Detect end of chunked body: \r\n0\r\n\r\n */
		if (memmem(buf, n, "\r\n0\r\n\r\n", 7))
		{
			seen_final_chunk = 1;
			break;
		}
	}

	return seen_final_chunk ? 0 : -1;
}

int handle_request(int client_fd, ParsedRequest *req, char *cache_key)
{
	int server_fd;
	char buf[MAX_BYTES];

	/* ---------- CONNECT TO ORIGIN ---------- */
	int port = req->port ? atoi(req->port) : 80;
	server_fd = connectRemoteServer(req->host, port);
	if (server_fd < 0)
		return -1;

	/* ---------- BUILD REQUEST TO ORIGIN ---------- */
	char reqbuf[MAX_BYTES];
	int offset = 0;

	offset += snprintf(reqbuf + offset, sizeof(reqbuf) - offset, "GET %s %s\r\n", req->path, req->version);

	ParsedHeader_set(req, "Connection", "close");

	if (!ParsedHeader_get(req, "Host"))
		ParsedHeader_set(req, "Host", req->host);

	ParsedRequest_unparse_headers(req, reqbuf + offset, sizeof(reqbuf) - offset);

	send(server_fd, reqbuf, strlen(reqbuf), 0);

	/* ---------- READ RESPONSE HEADERS ---------- */
	char header_buf[MAX_BYTES];
	memset(header_buf, 0, sizeof(header_buf));

	int header_len = read_response_headers(server_fd, header_buf, sizeof(header_buf));
	if (header_len < 0)
	{
		close(server_fd);
		return -1;
	}

	send(client_fd, header_buf, header_len, 0);

	/* ---------- PARSE RESPONSE ---------- */
	ParsedResponse res;
	if (ParsedResponse_parse(&res, header_buf, header_len) < 0)
	{
		close(server_fd);
		return -1;
	}

	/* ---------- FORWARD RESPONSE BODY ---------- */
	if (res.chunked)
	{
		forward_chunked(server_fd, client_fd);
	}
	else if (res.content_length >= 0)
	{
		int remaining = res.content_length;
		while (remaining > 0)
		{
			int to_read = remaining > MAX_BYTES ? MAX_BYTES : remaining;
			int n = recv(server_fd, buf, to_read, 0);
			if (n <= 0)
				break;
			send(client_fd, buf, n, 0);
			remaining -= n;
		}
	}
	else
	{
		/* HTTP/1.0 or no framing → read until close */
		int n;
		while ((n = recv(server_fd, buf, sizeof(buf), 0)) > 0)
			send(client_fd, buf, n, 0);
	}

	/* ---------- CLEANUP ---------- */
	ParsedResponse_destroy(&res);
	close(server_fd);
	return 0;
}

void *worker_thread(void *arg)
{
	(void)arg;

	while (1)
	{
		int socket = dequeue_job(&job_queue);
		printf("Worker %lu handling socket %d\n", pthread_self(), socket);

		int bytes_send_client, len;
		char *buffer = (char*)calloc(MAX_BYTES, sizeof(char));
		if (!buffer)
		{
			close(socket);
			continue;
		}

		bytes_send_client = recv(socket, buffer, MAX_BYTES, 0);
		while (bytes_send_client > 0)
		{
			len = strlen(buffer);
			if (strstr(buffer, "\r\n\r\n") == NULL)
			{
				bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len, 0);
			}
			else
			{
				break;
			}
		}

		char *tempReq = strdup(buffer);
		if (!tempReq)
		{
			free(buffer);
			close(socket);
			continue;
		}

		// private result, not accesible by other threads
		cache_result_t* temp = find_cache_element(tempReq);

		if (temp != NULL)
		{
			int pos = 0;
			while (pos < temp->len)
			{
				int chunk = (temp->len - pos > MAX_BYTES)? MAX_BYTES: temp->len - pos;

				send(socket, temp->data + pos, chunk, 0);
				pos += chunk;
			}
			free(temp->data);
			free(temp);
			printf("Data retrieved from cache.\n");
		}
		else if (bytes_send_client > 0)
		{
			ParsedRequest *req = ParsedRequest_create();
			if (ParsedRequest_parse(req, buffer, strlen(buffer)) < 0)
			{
				sendErrorMessage(socket, 400);
			}
			else if (!strcmp(req->method, "GET"))
			{
				if (req->host && req->path &&checkHTTPversion(req->version))
				{
					int h_req = handle_request(socket, req, tempReq);
					if (h_req < 0)
						sendErrorMessage(socket, 500);
					printf("Worker %lu finished socket %d\n", pthread_self(), socket);
				}
				else
				{
					sendErrorMessage(socket, 400);
				}
			}
			else
			{
				sendErrorMessage(socket, 501);
			}
			ParsedRequest_destroy(req);
		}
		else if (bytes_send_client == 0)
		{
			printf("Client disconnected.\n");
		}
		else
		{
			perror("recv");
		}

		shutdown(socket, SHUT_RDWR);
		close(socket);
		free(buffer);
		free(tempReq);
	}

	return NULL;
}

void init_cache_buckets()
{
	for (int i = 0; i < N_BUCKETS; i++)
	{
		cache_buckets[i].head = NULL;
		cache_buckets[i].cache_size = 0;
		pthread_rwlock_init(&cache_buckets[i].lock, NULL);
	}
}

// djb2 hash function for strings, returns an unsigned long hash value for the given string
static unsigned long hash_url(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

// Get the cache bucket for a given URL by hashing the URL and using bitmasking to find the appropriate bucket index
static inline cache_bucket_t *get_bucket(const char *url)
{
	return &cache_buckets[hash_url(url) & (N_BUCKETS - 1)];
}

void send_503(int client_fd)
{
	const char *response =
		"HTTP/1.1 503 Service Unavailable\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 19\r\n"
		"Connection: close\r\n"
		"Retry-After: 5\r\n"
		"\r\n"
		"Server overloaded\n";

	send(client_fd, response, strlen(response), 0);
}



// -------------------MAIN------------------------
int main(int argc, char **argv)
{
	int client_socketId, client_len;
	struct sockaddr_in server_addr, client_addr;
	init_cache_buckets();
	job_queue_init(&job_queue, JOB_QUEUE_MAX);
	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		pthread_create(&workers[i], NULL, worker_thread, NULL);
	}

	if (argc == 2)
		PORT = atoi(argv[1]);
	else
	{
		printf("Too few arguments.\n");
		exit(1);
	}
	printf("Starting Proxy Server on port: %d\n", PORT);
	proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
	if (proxy_socketId < 0)
	{
		perror("Failed to create a socket.\n");
		exit(1);
	}
	int reuse = 1;
	if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
	{
		perror("Setsockoption failed.\n");
		exit(1);
	}
	bzero((char *)&server_addr, sizeof(server_addr));
	// internet address family
	server_addr.sin_family = AF_INET;
	// converts network byte order from network byte order to  host machine byte order. network byte order is big endian, host may be different.
	server_addr.sin_port = htons(PORT);
	// Address to accept any incoming messages.
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
	{
		perror("Port is not available.\n");
		exit(1);
	}
	printf("Binding on port %d\n", PORT);
	int listen_status = listen(proxy_socketId, LISTEN_BACKLOG );

	if (listen_status < 0)
	{
		perror("Error when listening.\n");
	}
	printf("Listening on port %d\n", PORT);

	// int connected_socketId[MAX_CLIENTS];
	// int i = 0;
	while (1)
	{
		client_len = sizeof(client_addr);
		bzero((char *)&client_addr, client_len);
		client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
		if (client_socketId < 0)
		{
			if(errno == EINTR)
				continue; // Interrupted by signal, retry accept
			perror("accept");
			continue;
		}

		struct sockaddr_in *client_ptr = (struct sockaddr_in *)&client_addr;
		struct in_addr ip_addr = client_ptr->sin_addr;
		char str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
		// printf("Client is connected with port: %d and ip address: %s\n", ntohs(client_addr.sin_port), str);

		if(!enqueue_job(&job_queue, client_socketId)){
			send_503(client_socketId);
			close(client_socketId);
			printf("Job queue is full, rejected connection from %s:%d\n", str, ntohs(client_addr.sin_port));
		}
		else{
			printf("Enqueued job for client %s:%d\n", str, ntohs(client_addr.sin_port));
		}
	}
	close(proxy_socketId);
	return 0;
}

// -----------------------------------------------
cache_result_t *find_cache_element(char *url)
{
	cache_bucket_t* bucket = get_bucket(url);
	cache_element *website = NULL;
	cache_result_t *result = NULL;

	/* READ lock lookup + copy */
	pthread_rwlock_rdlock(&bucket->lock);

	// website = head;
	website = bucket->head;

	while (website != NULL)
	{
		if (!strcmp(website->url, url))
		{
			result = (cache_result_t *)malloc(sizeof(cache_result_t));
			if (!result)
				break;

			result->len = website->len;
			result->data = (char *)malloc(website->len);
			if (!result->data)
			{
				free(result);
				result = NULL;
				break;
			}

			memcpy(result->data, website->data, website->len);
			break;
		}
		website = website->next;
	}

	pthread_rwlock_unlock(&bucket->lock);

	/* WRITE lock update LRU metadata */
	if (website != NULL)
	{
		pthread_rwlock_wrlock(&bucket->lock);
		website->lru_time_track = time(NULL);
		pthread_rwlock_unlock(&bucket->lock);
	}

	return result;
}

int add_cache_element(char *data, int size, char *url)
{
	int element_size = size + 1 + strlen(url) + sizeof(cache_element);

	/* Too large to cache */
	if (element_size > MAX_ELEMENT_SIZE)
		return 0;
	
	cache_bucket_t* bucket = get_bucket(url);
	pthread_rwlock_wrlock(&bucket->lock);

	/* Evict until there is space */
	while (bucket->cache_size + element_size > MAX_SIZE)
	{
		remove_cache_element(bucket); // MUST assume write lock is held
	}

	cache_element *element = (cache_element *)malloc(sizeof(cache_element));
	if (!element)
	{
		pthread_rwlock_unlock(&bucket->lock);
		return 0;
	}

	element->data = (char *)malloc(size + 1);
	element->url = (char *)malloc(strlen(url) + 1);

	if (!element->data || !element->url)
	{
		free(element->data);
		free(element->url);
		free(element);
		pthread_rwlock_unlock(&bucket->lock);
		return 0;
	}

	memcpy(element->data, data, size);
	element->data[size] = '\0';
	strcpy(element->url, url);

	element->len = size;
	element->lru_time_track = time(NULL);

	element->next = bucket->head;
	bucket->head = element;

	bucket->cache_size += element_size;

	pthread_rwlock_unlock(&bucket->lock);
	return 1;
}

void remove_cache_element(cache_bucket_t* bucket)
{
	/* ASSUMES write lock already held */

	if (bucket->head == NULL)
		return;

	cache_element *curr = bucket->head;
	cache_element *prev = NULL;

	cache_element *lru = bucket->head;
	cache_element *lru_prev = NULL;

	while (curr != NULL)
	{
		if (curr->lru_time_track < lru->lru_time_track)
		{
			lru = curr;
			lru_prev = prev;
		}
		prev = curr;
		curr = curr->next;
	}

	/* Remove LRU element */
	if (lru_prev == NULL)
		bucket->head = lru->next;
	else
		lru_prev->next = lru->next;

	bucket->cache_size -= (lru->len + sizeof(cache_element) + strlen(lru->url) + 1);

	free(lru->data);
	free(lru->url);
	free(lru);
}

// -----------------------------------------------
void job_queue_init(job_queue_t *q, int max_size)
{
	q->front = q->rear = NULL;
	q->size = 0;
	q->max_size = max_size;
	pthread_mutex_init(&q->mutex, NULL);
	pthread_cond_init(&q->not_empty, NULL);
}

int enqueue_job(job_queue_t *q, int client_fd)
{
	pthread_mutex_lock(&q->mutex);

	if (q->size >= q->max_size)
	{
		pthread_mutex_unlock(&q->mutex);
		return 0;
	}
	job_t *job = (job_t *)malloc(sizeof(job_t));
	job->client_fd = client_fd;
	job->next = NULL;

	if(q->rear)
		q->rear->next = job;
	else
		q->front = job;
	
	q->rear = job;
	q->size++;

	pthread_cond_signal(&q->not_empty);
	pthread_mutex_unlock(&q->mutex);
	return 1;
}

int dequeue_job(job_queue_t *q)
{
	pthread_mutex_lock(&q->mutex);
	while (q->size == 0)
		pthread_cond_wait(&q->not_empty, &q->mutex);

	job_t *job = q->front;
	q->front = job->next;

	if (!q->front)
		q->rear = NULL;

	q->size--;

	pthread_mutex_unlock(&q->mutex);

	int fd = job->client_fd;
	free(job);
	return fd;
}
