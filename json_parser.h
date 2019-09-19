#ifndef __JON_PARSER_H_
#define __JON_PARSER_H_

#include "json_jsmn.h" 

#ifdef __cplusplus
extern "C" {
#endif
    
typedef int (*json_array_element_callback_t)(int index, jsmntype_t type, void *value, int len, void *callback_args);

int json_parse
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		const char **keys_filter_list,
		json_jsmntok_t *json_jsmntok, int json_jsmntok_count
	);

int json_parse_object
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		json_jsmn_object_t *json_jsmn_objects, int json_jsmn_object_count
	);

int json_parse_object_fmt
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		int objs_count, ...
	);

int json_parse_array
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		const char *name,
		json_array_element_callback_t callback, void *callback_args
	);

int json_parse_fmt
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		const char **keys_filter_list,
		int json_jsmntok_count, ...
	);

    
#ifdef __cplusplus
}
#endif

#endif  // __JON_PARSER_H_
