#ifndef __JSON_JSMN_H_
#define __JSON_JSMN_H_

#include <stddef.h>
#include "jsmn/jsmn.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	jsmntype_t t_value_type;
	jsmntok_t *t_key;
	jsmntok_t *t_value;
	int t_count;
}json_jsmntok_t;

typedef void (*json_jsmntok_callback_t)(json_jsmntok_t *json_jsmntok, const char *js, void *args);

enum jsonstatus
{
	JSON_JSMN_EMPTY,
	JSON_JSMN_VALID,
	JSON_JSMN_INVALID
};

typedef struct json_jsmn_object json_jsmn_object_t;
typedef void (*json_jsmn_object_callback_t)(struct json_jsmn_object *jobj, const char *js, jsmntok_t *t);

struct json_jsmn_object{
    const char *key;
    void *value;
    int size;
    jsmntype_t type;
    enum jsonstatus status;
    json_jsmn_object_callback_t callback;
};

typedef struct
{
	const char *js;
	const jsmntok_t *tokens;
	unsigned int token_count;
}json_jsmn_t;

#define jsmntok_strcmp(js, t, s)		strncmp((const char *)((js) + (t)->start), s, (t)->end - (t)->start)
#define jsmntok_strncasecmp(js, t, s)	strncasecmp((const char *)((js) + (t)->start), s, (t)->end - (t)->start)
#define jsmntok_get_offset(t)		(t->start)
#define jsmntok_get_size(t)			(t->end - t->start)

#define jsmntok_get_copy_size(t, buffer_size)	\
	(((t)->start + (buffer_size) + 1) < (t)->end ? ((t)->end - (t)->start):((buffer_size)-1))

static inline int jsmntok_strlcpy(const char *js, const jsmntok_t *t, char *s, size_t num)	\
{
	if(num > 1)
	{
		num = jsmntok_get_copy_size(t, num);
		strncpy(s, (const char *)js + t->start, num);
		s[num] = '\0';
		return num;
	}
	return 0;
}

int json_jsmn_parse
	(
		json_jsmn_t *jjs,
		const char **json_jsmntok_keys,
		json_jsmntok_t *json_jsmntok, int json_jsmntok_count
	);
int json_jsmn_parse_va_list
	(
		json_jsmn_t *jjs,
		const char **keys_filter_list,				// key filter list
		int json_jsmntok_count, va_list vargs		// output vargs
	);

int json_jsmn_parse_fmt
	(
		json_jsmn_t *jjs,
		const char **keys_filter_list,					// key filter list
		int json_jsmntok_count, ...
	);


int json_jsmn_parse_object
	(
		json_jsmn_t *jjs,
		json_jsmn_object_t *objs, int objs_count
	);

int json_jsmn_parse_object_va_list
	(
		json_jsmn_t *jjs,
		int objs_count, va_list args
	);

int json_jsmn_parse_object_fmt
	(
		json_jsmn_t *jjs,
		int objs_count, ...
	);

#ifdef __cplusplus
}
#endif

#endif /* __JSON_JSMN_H_ */
