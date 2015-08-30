#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <uv.h>
#include <curl/curl.h>
#include <oauth.h>

#include "stream.h"

struct bear_stream_s{
	uv_loop_t *loop;
	CURLM *curl_handle;
	uv_timer_t timeout;
	uv_poll_t poll_handle;
	curl_socket_t sockfd;
	char const* c_key;
	char const* c_sec;
	char const* t_key;
	char const* t_sec;
};

void create_curl_context(bear_stream_t *stream,curl_socket_t sockfd)
{
  stream->sockfd = sockfd;

  uv_poll_init_socket(stream->loop, &stream->poll_handle, sockfd);
  stream->poll_handle.data = stream;
}

int find_delimiter(char const* str,size_t len,size_t *beg,size_t *end){
	*beg = 0;
	*end = 0;
	for(size_t i = 0;i < len - 1;i++){
		if(str[i] == '\r' && str[i+1] == '\n'){
			if(!*beg){
				*beg = i;
			}
			*end = i + 2;
			//i += 2;
			i++; //Care i++ at for statement
		}
		else if(*beg){
			break;
		}
	}
	return *end - *beg;
}

size_t curl_write_cb(char *ptr,size_t size,size_t number,void *userdat){
	static char* buffer = NULL;
	static size_t buffer_len = 0;

	char *tmp = realloc(buffer,buffer_len + size * number);
	if(!tmp){
		free(buffer);
		buffer = NULL;
		buffer_len = 0;
		return size * number;
	}
	buffer = tmp;
	memcpy(&buffer[buffer_len],ptr,size * number);
	buffer_len += size * number;

	size_t begin_json = 0;
	size_t begin_delim,end_delim;
	while(find_delimiter(&buffer[begin_json],buffer_len - begin_json,&begin_delim,&end_delim)){
		if(begin_delim > 0){
			void (*func)(char const*,size_t);
			func = userdat;
			func(&buffer[begin_json],begin_delim);
		}
		begin_json += end_delim;
		if(begin_json >= buffer_len){
			break;
		}
	}
	

	if(begin_json){
		if(begin_json < buffer_len){
			tmp = malloc(buffer_len - begin_json);
			memcpy(tmp,&buffer[begin_json],buffer_len - begin_json);
			free(buffer);
			buffer = tmp;
			buffer_len -= begin_json;
		}
		else{
			//printf("fed all the buffer\n");
			free(buffer);
			buffer = NULL;
			buffer_len = 0;
		}
	}
	return size * number;
}

//メッセージが来ているか見て，通信が成功したならばハンドルを消す
void check_multi_info(bear_stream_t *stream)
{
  //int running_handles;
  char *done_url;
  CURLMsg *message;
  int pending;
  //FILE *file;

  while((message = curl_multi_info_read(stream->curl_handle, &pending))) {
    switch(message->msg) {
    case CURLMSG_DONE:
      curl_easy_getinfo(message->easy_handle, CURLINFO_EFFECTIVE_URL,
                        &done_url);
	  printf("result: %d\n",message->data.result);
	  
	  //curl ok
	  if(!message->data.result){
		printf("%s DONE\n", done_url);
	  }
	  else{
		printf("error: %s\n",curl_easy_strerror(message->data.result));
	  }

      curl_multi_remove_handle(stream->curl_handle, message->easy_handle);
      curl_easy_cleanup(message->easy_handle);
      break;

    default:
      fprintf(stderr, "CURLMSG default\n");
      break;
    }
  }
}

//libuv polling call back
//as soon as an event detected this function will be called.
void curl_perform(uv_poll_t *req, int status, int events)
{
  (void)status; //unused
  int running_handles;
  int flags = 0;
  
  bear_stream_t *stream = (bear_stream_t *) req->data;

  uv_timer_stop(&stream->timeout);

  if(events & UV_READABLE)
    flags |= CURL_CSELECT_IN;
  if(events & UV_WRITABLE)
    flags |= CURL_CSELECT_OUT;

  curl_multi_socket_action(stream->curl_handle,stream->sockfd, flags,
                           &running_handles);

  check_multi_info(stream);
}

void on_timeout(uv_timer_t *req)
{
  int running_handles;
  bear_stream_t *stream = (bear_stream_t*) req->data;

  curl_multi_socket_action(stream->curl_handle, CURL_SOCKET_TIMEOUT, 0,
                           &running_handles);
  check_multi_info(stream);
}

void start_timeout(CURLM *multi, long timeout_ms, void *userp)
{
	(void)multi; //unused
	bear_stream_t *stream = (bear_stream_t*) userp;
	if(timeout_ms < 0){ //timer should be deleted.
		uv_timer_stop(&stream->timeout);
		return;
	}
  if(timeout_ms == 0)
    timeout_ms = 1; /* 0 means directly call socket_action, but we'll do it in
                       a bit */
  uv_timer_start(&stream->timeout, on_timeout, timeout_ms, 0);
}

int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp,
                  void *socketp)
{
	(void)easy; //unused
	bear_stream_t *stream = (bear_stream_t*) userp;
  if(action == CURL_POLL_IN || action == CURL_POLL_OUT) {
    if(socketp) {
    }
    else {
      create_curl_context(stream,s);
    }
  }

  switch(action) {
  case CURL_POLL_IN:
    uv_poll_start(&stream->poll_handle, UV_READABLE, curl_perform);
    break;
  case CURL_POLL_OUT:
    uv_poll_start(&stream->poll_handle, UV_WRITABLE, curl_perform);
    break;
  case CURL_POLL_REMOVE:
    if(socketp) {
      uv_poll_stop(&stream->poll_handle);
    }
    break;
  default:
    abort();
  }

  return 0;
}

void add_userstream_handle(bear_stream_t *stream,char const* endpoint,bear_stream_handler handler){
	CURL *easy_handle = curl_easy_init();
	char *url;
	url = oauth_sign_url2(endpoint,NULL,OA_HMAC,NULL,stream->c_key,stream->c_sec,stream->t_key,stream->t_sec);
	curl_easy_setopt(easy_handle,CURLOPT_WRITEFUNCTION,curl_write_cb);
	curl_easy_setopt(easy_handle,CURLOPT_WRITEDATA,handler);
	curl_easy_setopt(easy_handle, CURLOPT_URL, url);
	curl_easy_setopt(easy_handle,CURLOPT_SSL_VERIFYPEER,0L);
	curl_multi_add_handle(stream->curl_handle, easy_handle);
}

void bear_init(){
	if(curl_global_init(CURL_GLOBAL_ALL)) {
		fprintf(stderr, "Could not init cURL\n");
	}
}

bear_stream_t *create_bear_stream(uv_loop_t *loop,char const* c_key,char const* c_sec,char const* t_key,char const* t_sec){
	bear_stream_t *stream;
	stream = (bear_stream_t*) malloc(sizeof *stream);

	stream->loop = loop;
	uv_timer_init(stream->loop, &stream->timeout);
	stream->timeout.data = stream;

	stream->curl_handle = curl_multi_init();
	curl_multi_setopt(stream->curl_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);
	curl_multi_setopt(stream->curl_handle,CURLMOPT_SOCKETDATA,stream);
	curl_multi_setopt(stream->curl_handle, CURLMOPT_TIMERFUNCTION, start_timeout);
	curl_multi_setopt(stream->curl_handle,CURLMOPT_TIMERDATA,stream);

	stream->c_key = c_key;
	stream->c_sec = c_sec;
	stream->t_key = t_key;
	stream->t_sec = t_sec;

	return stream;
}
