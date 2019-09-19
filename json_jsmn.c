#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "json_jsmn.h"

#ifndef assert
#define assert(c)
#endif

#ifdef JSON_JSMN_DEBUG_ENABLED
#ifndef debugPrintf
#define debugPrintf    				printf
#define debugPrintln(fmt,args...)   debugPrintf(fmt "%s", ## args, "\r\n")
#else
#define debugPrintln(fmt,args...)   debugPrintf(fmt "%s", ## args, "\r\n")
#endif
#else
#define debugPrintf(...)
#define debugPrintln(...)
#endif

typedef enum { START, KEY, VALUE, SKIP, STOP } parse_state;

typedef int (*json_jsmn_get_key_t)
		(
			const char *js,		// input json
			const jsmntok_t *t,	// input token
			void *args
		);
typedef int (*json_jsmn_get_value_t)
		(
			const char *js,
			const jsmntok_t *t, size_t t_count,
			void *args
		);

#define jsmn_object_size(t,t_count)	json_jsmn_get_value(NULL,t,t_count,NULL,0)
static int json_jsmn_get_value
	(
		const char *js,
		const jsmntok_t *t, size_t t_count,
		void *out, int size
	)
{
	int i, j;

	if (t_count == 0)
	{
		return 0;
	}

	switch(t->type)
	{
	case JSMN_STRING:
		if(js && out && size)
		{
			i = t->end - t->start + 1;
			if(size > i)
				size = i;
			strlcpy((char *)out, js + t->start, size);
		}
		return 1;
	case JSMN_PRIMITIVE:
		if(js && out && size)
		{
			long n;

			if(0 == strncmp(js + t->start, "true", 4))
			{
				n = 1;
			}
			else if(0 == strncmp(js + t->start, "true", 4))
			{
				n = 0;
			}
			else
			{
				n = strtol(js + t->start, NULL, 10);
			}

			switch(size)
			{
				case sizeof(uint64_t):
				{
					int64_t value = n;
					memcpy(out, &value, size);
				}
				break;

				case sizeof(int32_t):
				{
					int32_t value = n;
					memcpy(out, &value, size);
				}
				break;

				case sizeof(int16_t):
				{
					int16_t value = n;
					memcpy(out, &value, size);
				}

				case sizeof(int8_t):
				{
					int8_t value = n;
					memcpy(out, &value, size);
				}
			}
		}
		return 1;

	case JSMN_OBJECT:
		// bypass
		j = 0;
		for (i = 0; i < t->size; i++)
		{
			// name
			j+= json_jsmn_get_value(js, t+1+j, t_count-j, NULL, 0);

			//value
			j+= json_jsmn_get_value(js, t+1+j, t_count-j, NULL, 0);
		}
		return j+1;

	case JSMN_ARRAY:
		j = 0;
		for (i = 0; i < t->size; i++)
		{
			j+= json_jsmn_get_value(js, t+1+j, t_count-j, NULL, 0);
		}
		return j+1;

	default:
		return 0;
	}
}

static int json_jsmn_parse_core
	(
		json_jsmn_t *jjs,
		json_jsmn_get_key_t get_key_callback,
		json_jsmn_get_value_t get_value_callback,
		void *args
	)
{
	int i, t_skip, t_skip1, n;
	parse_state state = START;
	int token_size = 0;


	for (
			n = 0, i = 0, t_skip = 1;
			i < jjs->token_count;
			i += t_skip
		)
	{
		const jsmntok_t *t = &jjs->tokens[i];

		// Should never reach uninitialized tokens
		assert(t->start != -1 && t->end != -1);

		switch (state)
		{
			case START:
				if (t->type != JSMN_OBJECT)
				{
					debugPrintln("Invalid object(%d): root element must be an object.", t->type);
				}

				if (!t->size)
				{
					state = START;
					debugPrintln("Empty object.");
				}
				else
				{
					state = KEY;
					token_size = t->size;
				}

				t_skip = 1;
				break;

			case KEY:
				token_size--;
				t_skip = 1;

				if (t->type != JSMN_STRING)
				{
					debugPrintln("Invalid object(%d): object keys must be strings.", t->type);
				}

				if(!get_key_callback(jjs->js, t, args))
				{
					state = SKIP;
					debugPrintln("skip token: %.*s", t->end - t->start, jjs->js+t->start);
				}
				else
				{
					state = VALUE;
					debugPrintln("add token: %.*s", t->end - t->start, jjs->js+t->start);
				}

				break;

			case SKIP:
				t_skip = jsmn_object_size(t, jjs->token_count-t_skip);
//            	token_size -= 1;
				state = KEY;

				if (token_size == 0)
					state = START;
				break;

			case VALUE:
				t_skip1 = get_value_callback
							(
								jjs->js,
								t, jjs->token_count-t_skip,
								args
							);
				if(t_skip1)
				{
					t_skip = t_skip1;
					n++;
				}
				else
				{
					t_skip = jsmn_object_size(t, jjs->token_count-t_skip);
				}

				state = KEY;

				if (token_size == 0)
					state = START;

				break;

			case STOP:
				// Just consume the tokens
				break;

			default:
				debugPrintln("Invalid state %u", state);
		}
	}
//    assert_fmt(n <= json_jsmntok_count, "invalid return (%d)", n);
	return n;
}

struct parse_jsmntok_args
{
	int count;
	int index;
	const char **keys_filter_list;
	json_jsmntok_t *json_jsmntok_list;
};
static int parse_get_key
	(
		const char *js,
		jsmntok_t *t,
		struct parse_jsmntok_args *jargs
	)
{
	const char **keys_filter_list;

	if(jargs->index >= jargs->count)
	{
		return 0;
	}

	if(!jargs->keys_filter_list)
	{
		jargs->json_jsmntok_list[jargs->index].t_key = t;
		return 1;
	}

	keys_filter_list = jargs->keys_filter_list;
	while(*keys_filter_list)
	{
		if(0 == jsmntok_strcmp(js, t, *keys_filter_list++))
		{
			jargs->json_jsmntok_list[jargs->index].t_key = t;
			return 1;
		}
	}

	return 0;
}
static int parse_get_value
	(
		const char *js,
		jsmntok_t *t, size_t t_count,
		struct parse_jsmntok_args *jargs
	)
{
	int t_skip;

	if(jargs->index >= jargs->count)
	{
		return 0;
	}

	jargs->json_jsmntok_list[jargs->index].t_value_type = t->type;
	jargs->json_jsmntok_list[jargs->index].t_value = t;
	jargs->json_jsmntok_list[jargs->index].t_count = t_skip = jsmn_object_size(t, t_count);
	jargs->index++;
	return t_skip;
}
int json_jsmn_parse
		(
			json_jsmn_t *jjs,
			const char **keys_filter_list,							// key filter list
			json_jsmntok_t *json_jsmntok, int json_jsmntok_count	// output buffer
		)
{
	struct parse_jsmntok_args parse_jsmntok_args;

	parse_jsmntok_args.index = 0;
	parse_jsmntok_args.count = json_jsmntok_count;
	parse_jsmntok_args.json_jsmntok_list = json_jsmntok;
	parse_jsmntok_args.keys_filter_list = keys_filter_list;
	return json_jsmn_parse_core
				(
					jjs,
					(json_jsmn_get_key_t)parse_get_key,
					(json_jsmn_get_value_t)parse_get_value,
					&parse_jsmntok_args
				);
}

struct parse_object_args
{
	int count;
	int index;
	json_jsmn_object_t *jobj;
};
static int parse_object_get_key_args_callback
	(
		const char *js,
		jsmntok_t *t,
		struct parse_object_args *jvargs
	)
{
	int i;

	jvargs->index = -1;
	for(i = 0; i < jvargs->count; i++)
	{
		if (0 == jsmntok_strcmp(js, t, jvargs->jobj[i].key))
		{
			jvargs->index = i;
			return 1;
		}
	}
	return 0;
}

static int parse_object_get_value_args_callback
	(
		const char *js,
		jsmntok_t *t, size_t t_count,
		struct parse_object_args *jvargs
	)
{
	int t_skip=0;

	if(jvargs->index == -1)
	{
		return 0;
	}
	t_skip = json_jsmn_get_value
			(
				js,
				t, t_count,
				jvargs->jobj[jvargs->index].value, jvargs->jobj[jvargs->index].size
			);

	if(jvargs->jobj[jvargs->index].type == t->type)
	{
		jvargs->jobj[jvargs->index].status = JSON_JSMN_VALID;
		jvargs->count--;
		if(jvargs->jobj[jvargs->index].callback)
		{
			jvargs->jobj[jvargs->index].callback(jvargs->jobj, js, t);
		}
		return t_skip;
	}
	else
	{
		jvargs->jobj[jvargs->index].status = JSON_JSMN_INVALID;
		return 0;
	}
}

int json_jsmn_parse_object
	(
		json_jsmn_t *jjs,
		json_jsmn_object_t *objs, int objs_count
	)
{
	struct parse_object_args parse_object_args;

	parse_object_args.index = -1;
	parse_object_args.count = objs_count;
	parse_object_args.jobj = objs;
	return json_jsmn_parse_core
			(
				jjs,
				(json_jsmn_get_key_t)parse_object_get_key_args_callback,
				(json_jsmn_get_value_t)parse_object_get_value_args_callback,
				&parse_object_args
			);
}

#if __STDC_VERSION__ >= 199901L
struct parse_jsmntok_vargs
{
	int count;
	const char **keys_filter_list;
	va_list vargs;
	json_jsmntok_t *json_jsmntok;
};
static int parse_get_key_vargs_callback
	(
		const char *js,
		jsmntok_t *t,
		struct parse_jsmntok_vargs *jargs
	)
{
	const char **keys_filter_list;
	va_list args1;
	int i;
	json_jsmntok_t *json_jsmntok;

	va_copy(args1, jargs->vargs);
	for(i = 0; i < jargs->count; i++)
	{
		json_jsmntok = va_arg(jargs->vargs, json_jsmntok_t *);

		if(json_jsmntok)
		{
			if(!jargs->keys_filter_list)
			{
				jargs->json_jsmntok = json_jsmntok;
				return 1;
			}

			keys_filter_list = jargs->keys_filter_list;
			while(*keys_filter_list)
			{
				if(0 == jsmntok_strcmp(js, t, *keys_filter_list++))
				{
					jargs->json_jsmntok = json_jsmntok;
					jargs->json_jsmntok->t_key = t;
					return 1;
				}
			}
		}
	}
	va_end(args1);

	return 0;
}
static int parse_get_value_vargs_callback
	(
		const char *js,
		jsmntok_t *t, size_t t_count,
		struct parse_jsmntok_vargs *jargs
	)
{
	int t_skip;

	if(!jargs->json_jsmntok)
	{
		return 0;
	}

	jargs->json_jsmntok->t_value_type = t->type;
	jargs->json_jsmntok->t_value = t;
	jargs->json_jsmntok->t_count = t_skip = jsmn_object_size(t, t_count);
	jargs->count--;
	return t_skip;
}
static void parse_init_va_list(va_list vargs)
{
	va_list args1;
	json_jsmntok_t *json_jsmntok;

	va_copy(args1, vargs);
	json_jsmntok = va_arg(vargs, json_jsmntok_t *);
	if(json_jsmntok)
	{
		json_jsmntok->t_value_type = JSMN_UNDEFINED;
		json_jsmntok->t_key = NULL;
		json_jsmntok->t_value = NULL;
		json_jsmntok->t_count = 0;
	}
	va_end(args1);
}
int json_jsmn_parse_va_list
		(
			json_jsmn_t *jjs,
			const char **keys_filter_list,				// key filter list
			int json_jsmntok_count, va_list vargs		// output vargs
		)
{
	struct parse_jsmntok_vargs parse_jsmntok_vargs;

	parse_init_va_list(vargs);
	parse_jsmntok_vargs.count = json_jsmntok_count;
	parse_jsmntok_vargs.keys_filter_list = keys_filter_list;
	parse_jsmntok_vargs.vargs = vargs;
	parse_jsmntok_vargs.json_jsmntok = NULL;
	return json_jsmn_parse_core
				(
					jjs,
					(json_jsmn_get_key_t)parse_get_key_vargs_callback,
					(json_jsmn_get_value_t)parse_get_value_vargs_callback,
					&parse_jsmntok_vargs
				);
}
int json_jsmn_parse_fmt
	(
		json_jsmn_t *jjs,
		const char **keys_filter_list,					// key filter list
		int json_jsmntok_count, ...
	)
{
	int n;
	va_list args;

	va_start(args, json_jsmntok_count);
	n = json_jsmn_parse_va_list
			(
				jjs,
				keys_filter_list,
				json_jsmntok_count, args
			);
	va_end(args);

	return n;
}

struct json_jsmn_parse_object_vargs
{
	int count;
	va_list args;
	json_jsmn_object_t *jobj;
};
static int parse_object_get_key_vargs_callback
	(
		const char *js,
		jsmntok_t *t,
		struct json_jsmn_parse_object_vargs *jvargs
	)
{
	va_list args1;
	int i;
	json_jsmn_object_t *jobj;

	va_copy(args1, jvargs->args);
	for(i = 0; i < jvargs->count; i++)
	{
		jobj = va_arg(jvargs->args, json_jsmn_object_t *);
		debugPrintln("object: %s", jobj ? jobj->key:"null");
		if (jobj && 0 == jsmntok_strcmp(js, t, jobj->key))
		{
			jvargs->jobj = jobj;
			return 1;
		}
	}
	va_end(args1);
	return 0;
}
static int parse_object_get_value_vargs_callback
	(
		const char *js,
		jsmntok_t *t, size_t t_count,
		struct json_jsmn_parse_object_vargs *jvargs
	)
{
	int t_skip;

	if(jvargs->jobj && jvargs->jobj->type == t->type)
	{
		t_skip = json_jsmn_get_value(js, t, t_count, jvargs->jobj->value, jvargs->jobj->size);
		jvargs->jobj->status = JSON_JSMN_VALID;
		jvargs->count--;
		if(jvargs->jobj->callback)
		{
			jvargs->jobj->callback(jvargs->jobj, js, t);
		}
		return t_skip;
	}
	else
	{
		jvargs->jobj->status = JSON_JSMN_INVALID;
		return 0;
	}
}
static void parse_object_init_va_list(va_list vargs)
{
	va_list args1;
	json_jsmn_object_t *json_jsmn_object;

	va_copy(args1, vargs);
	json_jsmn_object = va_arg(vargs, json_jsmn_object_t *);
	if(json_jsmn_object)
	{
		json_jsmn_object->type = JSMN_UNDEFINED;
		json_jsmn_object->value = NULL;
		json_jsmn_object->status = JSON_JSMN_INVALID;
	}
	va_end(args1);
}
int json_jsmn_parse_object_va_list
	(
		json_jsmn_t *jjs,
		int objs_count, va_list vargs					// output args
	)
{
	struct json_jsmn_parse_object_vargs json_jsmn_parse_object_vargs;

	parse_object_init_va_list(vargs);
	json_jsmn_parse_object_vargs.count = objs_count;
	json_jsmn_parse_object_vargs.args = vargs;
	json_jsmn_parse_object_vargs.jobj = NULL;
	return json_jsmn_parse_core
			(
				jjs,
				(json_jsmn_get_key_t)parse_object_get_key_vargs_callback,
				(json_jsmn_get_value_t)parse_object_get_value_vargs_callback,
				&json_jsmn_parse_object_vargs
			);
}

int json_jsmn_parse_object_fmt
	(
		json_jsmn_t *jjs,
		int objs_count, ...
	)
{
	int n;
	va_list args;

	va_start(args, objs_count);
	n = json_jsmn_parse_object_va_list
			(
				jjs,
				objs_count, args
			);
	va_end(args);

	return n;
}
#endif // #if __STDC_VERSION__ >= 199901L

