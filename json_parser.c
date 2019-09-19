#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "json_parser.h"

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

static void jsmntok_print(const char *js, jsmntok_t *tok)
{
    int i;
    
    for(i=tok->start;i<tok->end;i++)
        debugPrintf("%c", js[i]);
}
#define object_print(js,key,value)	\
do{	\
	jsmntok_print(js, key);	\
	debugPrintf(" : ");	\
	jsmntok_print(js, value);	\
	debugPrintf("\r\n");	\
}while(0)

#define array_element_print(js,index,key,value)	\
do{	\
	jsmntok_print(js, key);	\
	debugPrintf("[%d] : ", index);	\
	jsmntok_print(js, value);	\
	debugPrintf("\r\n");	\
}while(0)
 
int json_parse_jsmn(jsmn_parser *parser, const char *js, unsigned int jslen, jsmntok_t *tokens, int tokcount)
{
    int rc;
    
    rc = jsmn_parse(parser, js, jslen, tokens, tokcount);
    if (0 > rc)
    {
        switch(rc)
        {
            /* Not enough tokens were provided */
        case JSMN_ERROR_NOMEM:
        	debugPrintln("jsmn_parse: JSMN_ERROR_NOMEM: Not enough tokens were provided");
            break;

            /* Invalid character inside JSON string */
        case JSMN_ERROR_INVAL:
            /* The string is not a full JSON packet, more bytes expected */
        	debugPrintln("jsmn_parse: JSMN_ERROR_INVAL: The string is not a full JSON packet, more bytes expected");
        	break;

        case JSMN_ERROR_PART:
        	debugPrintln("jsmn_parse: JSMN_ERROR_PART");
            break;

        default:
        	debugPrintln("jsmn_parse: error: %d", rc);
        	break;
        }
    }
    return rc;
}

int json_parse
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		const char **keys_filter_list,
		json_jsmntok_t *json_jsmntok, int json_jsmntok_count
	)
{
	int rc;
	jsmn_parser jsmn_parser_object;
	json_jsmn_t jjs;

	jsmn_init(&jsmn_parser_object);

    rc = json_parse_jsmn(&jsmn_parser_object, js, jslen, tokens, tokcount);
    if(0 > rc)
    {
        return rc;
    }

    debugPrintln("jsmn_parse(): token_count: %u", jsmn_parser_object.toknext);

    jjs.js = js;
	jjs.tokens = tokens;
	jjs.token_count = jsmn_parser_object.toknext;

	return json_jsmn_parse
			(
				&jjs,
				keys_filter_list,
				json_jsmntok, json_jsmntok_count
			);
}
 
int json_parse_object
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		json_jsmn_object_t *json_jsmn_objects, int json_jsmn_object_count
	)
{
	int i,rc;
	jsmn_parser jsmn_parser_object;
	json_jsmn_t jjs;

	jsmn_init(&jsmn_parser_object);
    
    rc = json_parse_jsmn(&jsmn_parser_object, js, jslen, tokens, tokcount);
    if(0 > rc)
    {
        return rc;
    }
    
    jjs.js = js;
    jjs.tokens = tokens;
    jjs.token_count = jsmn_parser_object.toknext;

	rc=json_jsmn_parse_object(&jjs, json_jsmn_objects, json_jsmn_object_count);

	for(i=0;i<json_jsmn_object_count;i++)
	{
		if(json_jsmn_objects[i].status == JSON_JSMN_VALID)
		{
			if(json_jsmn_objects[i].type == JSMN_STRING)
			{
				debugPrintln("jsmn_parse(): %s:%s", (char *)json_jsmn_objects[i].key, (char *)json_jsmn_objects[i].value);
			}
			else if(json_jsmn_objects[i].type == JSMN_PRIMITIVE)
			{
				long value;

				memset(&value, 0, sizeof(value));
				memcpy(&value, json_jsmn_objects[i].value, json_jsmn_objects[i].size > sizeof(value) ? sizeof(value):json_jsmn_objects[i].size);
				debugPrintln("jsmn_parse(): %s:%ld", (char *)json_jsmn_objects[i].key, value);
			}
		}
	}

	return 0;
}

int json_parse_array
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		const char *name,
		json_array_element_callback_t callback, void *callback_args
	)
{
	int i,j,rc;
	jsmn_parser jsmn_parser_object;
	const char *json_jsmntok_keys[2];
	json_jsmntok_t json_jsmntok;
	json_jsmn_t jjs;

	jsmn_init(&jsmn_parser_object);

    rc = json_parse_jsmn(&jsmn_parser_object, js, jslen, tokens, tokcount);
    if(0 > rc)
    {
        return rc;
    }

    jjs.js = js;
    jjs.tokens = tokens;
    jjs.token_count = jsmn_parser_object.toknext;

	json_jsmntok_keys[0] = name;
	json_jsmntok_keys[1] = NULL;
	rc = json_jsmn_parse(&jjs, json_jsmntok_keys, &json_jsmntok, 1);

	if(rc == 1 && json_jsmntok.t_value->type == JSMN_ARRAY)
    {
        if(json_jsmntok.t_count > 1)
        {
            for(rc=0,i=1;i<json_jsmntok.t_count;i++)
            {
                rc++;
                if(callback)
                {
                    int r = (*callback)(i-1,
                                        json_jsmntok.t_value[i].type,
                                        (void *)(js + json_jsmntok.t_value[i].start),
                                        json_jsmntok.t_value[i].end-json_jsmntok.t_value[i].start,
                                        callback_args
                                    );
                    if(r)
                    {
                        // stop request from callback
                        break;
                    }
                }
                array_element_print(js, i-1, json_jsmntok.t_key, &json_jsmntok.t_value[i]);
            }
        }
        else
        {
        	debugPrintln("jsmn_parse(): array is empty: %d", rc);
            rc = 0;
        }
    }
	else
	{
		debugPrintln("jsmn_parse(): invalid jsmn array: %d", rc);
		rc = -1;
	}

	return rc;
}


#if __STDC_VERSION__ >= 199901L
int json_parse_fmt
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		const char **keys_filter_list,
		int json_jsmntok_count, ...
	)
{
	int rc;
	jsmn_parser jsmn_parser_object;
	json_jsmn_t jjs;
	va_list args;

	jsmn_init(&jsmn_parser_object);

    rc = json_parse_jsmn(&jsmn_parser_object, js, jslen, tokens, tokcount);
    if(0 > rc)
    {
        return rc;
    }

    debugPrintln("jsmn_parse(): token_count: %u", jsmn_parser_object.toknext);

    jjs.js = js;
	jjs.tokens = tokens;
	jjs.token_count = jsmn_parser_object.toknext;

	va_start(args, json_jsmntok_count);
	rc = json_jsmn_parse_va_list
			(
				&jjs,
				keys_filter_list,
				json_jsmntok_count, args
			);
	va_end(args);

	return rc;
}

int json_parse_object_fmt
	(
		const char *js, unsigned int jslen,
		jsmntok_t *tokens, int tokcount,
		int objs_count, ...
	)
{
	int rc;
	jsmn_parser jsmn_parser_object;
	json_jsmn_t jjs;
	va_list args;

	jsmn_init(&jsmn_parser_object);

    rc = json_parse_jsmn(&jsmn_parser_object, js, jslen, tokens, tokcount);
    if(0 > rc)
    {
        return rc;
    }

    jjs.js = js;
	jjs.tokens = tokens;
	jjs.token_count = jsmn_parser_object.toknext;

	va_start(args, objs_count);
	rc = json_jsmn_parse_object_va_list
			(
				&jjs,
				objs_count, args
			);
	va_end(args);

	return rc;
}
#endif // #if __STDC_VERSION__ >= 199901L





