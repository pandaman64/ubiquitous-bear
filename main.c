#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "jansson.h"
#include "stream.h"

//printf format specifier for x64 size_t 
#include <inttypes.h>

//Consumer key/secret, Token key/secret
#include "keys.h"

void read_stream(char const* str,size_t len){
	//printf("read stream :%"PRIu64"\n",stream_body_length());
	//ちら見せ
	printf("length: %"PRIu64"\n",len);
	assert(str[0] != '\r');
	
	json_t *json;
	json_error_t err;
	json = json_loadb(str,len,0,&err);
	if(json){
		json_t *text,*created_at,*user,*user_name,*screen_name;
		text = json_object_get(json,"text");
		user = json_object_get(json,"user");
		created_at = json_object_get(json,"created_at");
		if(text && user && created_at){
			user_name = json_object_get(user,"name");
			screen_name = json_object_get(user,"screen_name");
			if(user_name && screen_name){
				printf("%s @%s\t\t%s\n",json_string_value(user_name),json_string_value(screen_name),json_string_value(created_at));
				printf("%s\n",json_string_value(text));
			}
		}
	}
	else{
		printf("%s\n",err.text);
	}
	printf("===================---------------\n");
	fflush(stdout);
}



int main(int argc, char **argv)
{
	char const* endpoint = "https://userstream.twitter.com/1.1/user.json";
	
	bear_init();

	uv_loop_t *loop = uv_default_loop();

	bear_stream_t *stream = create_bear_stream(loop,c_key,c_sec,t_key,t_sec);
	add_userstream_handle(stream,endpoint,read_stream);

	uv_run(loop, UV_RUN_DEFAULT);
	return 0;
}
