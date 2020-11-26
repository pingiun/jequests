#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <curl/curl.h>
#include <janet.h>

enum Method { Get, Options, Head, Post, Put, Patch, Delete };

size_t rstrip(uint8_t *buffer, size_t len) {
  while (isspace(buffer[len - 1])) {
    len = len - 1;
  }
  return len;
}

void optstringbuffer(const Janet *argv, int32_t argc, int32_t n,
                     const uint8_t **data, int32_t *length) {
  if (n >= argc || janet_checktype(argv[n], JANET_NIL)) {
    *data = NULL;
    *length = 0;
    return;
  }
  if (janet_checktype(argv[n], JANET_STRING)) {
    const uint8_t *str = janet_unwrap_string(argv[n]);
    *data = str;
    *length = janet_string_length(str);
    return;
  }
  if (janet_checktype(argv[n], JANET_BUFFER)) {
    JanetBuffer *buf = janet_unwrap_buffer(argv[n]);
    *data = buf->data;
    *length = buf->count;
    return;
  }
  janet_panic_type(argv[n], n, JANET_TFLAG_STRING | JANET_TFLAG_BUFFER);
}

void optarraytuple(const Janet *argv, int32_t argc, int32_t n,
                   const Janet **items, int32_t *length) {
  if (n >= argc || janet_checktype(argv[n], JANET_NIL)) {
    *items = NULL;
    *length = 0;
    return;
  }
  if (janet_checktype(argv[n], JANET_ARRAY)) {
    const JanetArray *array = janet_unwrap_array(argv[n]);
    *items = array->data;
    *length = array->count;
    return;
  }
  if (janet_checktype(argv[n], JANET_TUPLE)) {
    const Janet *tuple = janet_unwrap_tuple(argv[n]);
    *items = tuple;
    *length = janet_tuple_length(tuple);
    return;
  }
  janet_panic_type(argv[n], n, JANET_TFLAG_ARRAY | JANET_TFLAG_TUPLE);
}

enum Method which_method(const char *arg) {
  size_t n = janet_string_length(arg);
  if (strncmp(arg, "get", n) == 0)
    return Get;
  if (strncmp(arg, "options", n) == 0)
    return Options;
  if (strncmp(arg, "head", n) == 0)
    return Head;
  if (strncmp(arg, "post", n) == 0)
    return Post;
  if (strncmp(arg, "put", n) == 0)
    return Put;
  if (strncmp(arg, "patch", n) == 0)
    return Patch;
  if (strncmp(arg, "delete", n) == 0)
    return Delete;
  janet_panicf("unknown http method %s", arg);
}

size_t write_to_buffer(uint8_t *buffer, size_t size, size_t nmemb,
                       void *userdata) {
  assert(size == 1);
  JanetBuffer *ret_buffer = (JanetBuffer *)userdata;

  janet_buffer_push_bytes(ret_buffer, buffer, (int32_t)(nmemb));

  return nmemb;
}

size_t header_callback(uint8_t *buffer, size_t size, size_t nitems,
                       void *userdata) {
  assert(size == 1);
  JanetArray *ret_array = (JanetArray *)userdata;

  if (nitems > 0 && (buffer[0] == '\r' || buffer[0] == '\n')) {
    return nitems;
  }
  if (memcmp(buffer, "HTTP/", 5) == 0) {
    return nitems;
  }

  const uint8_t *line = janet_string(buffer, rstrip(buffer, nitems));

  janet_array_push(ret_array, janet_wrap_string(line));

  return nitems;
}

static Janet cfun_request(int32_t argc, Janet *argv) {
  janet_arity(argc, 2, 4);

  JanetKeyword method_str = janet_getkeyword(argv, 0);
  enum Method method = which_method((const char *)method_str);
  const char *url = janet_getcstring(argv, 1);
  const uint8_t *data;
  int32_t data_length;
  optstringbuffer(argv, argc, 2, &data, &data_length);

  if (method == Post && data == NULL)
    janet_panicf("data argument is required for POST requests");

  const Janet *headers;
  int32_t headers_length;
  optarraytuple(argv, argc, 3, &headers, &headers_length);

  CURL *curl = curl_easy_init();

  if (!curl)
    janet_panic("cannot get curl handle");

  CURLcode res;
  struct curl_slist *chunk = NULL;
  char errbuf[CURL_ERROR_SIZE];
  errbuf[0] = 0;

  JanetBuffer *buffer = janet_buffer(0);
  JanetTable *ret_table = janet_table(2);
  JanetArray *header_array = janet_array(0);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, header_array);

  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   "jequests (https://github.com/pingiun/jequests)");

  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  if (method == Post) {
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data_length);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
  }

  if (headers_length > 0) {
    int error = 0;
    int i;

    for (i = 0; i < headers_length; i++) {
      if (!janet_checktype(headers[i], JANET_STRING)) {
        error = 1;
        break;
      }
      const uint8_t *line = janet_unwrap_string(headers[i]);
      chunk = curl_slist_append(chunk, (char *)line);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    if (error) {
      curl_easy_cleanup(curl);
      curl_slist_free_all(chunk);
      janet_panicf("bad item, expected %T, got %v", JANET_TFLAG_STRING,
                   headers[i]);
    }
  }

  res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    curl_easy_cleanup(curl);
    if (chunk != NULL)
      curl_slist_free_all(chunk);
    size_t len = strlen(errbuf);
    if (len) {
      janet_panicf("%s", errbuf);
    } else {
      janet_panicf("%s", curl_easy_strerror(res));
    }
  }

  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  janet_table_put(ret_table, janet_ckeywordv("text"),
                  janet_wrap_buffer(buffer));
  janet_table_put(ret_table, janet_ckeywordv("status-code"),
                  janet_wrap_integer((int32_t)response_code));
  janet_table_put(ret_table, janet_ckeywordv("headers"),
                  janet_wrap_array(header_array));

  curl_easy_cleanup(curl);
  if (chunk != NULL)
    curl_slist_free_all(chunk);

  return janet_wrap_table(ret_table);
}

static Janet cfun_escape(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  JanetString str = janet_getstring(argv, 0);

  CURL *curl = curl_easy_init();
  if (!curl)
    janet_panic("cannot get curl handle");

  char *output =
      curl_easy_escape(curl, (const char *)str, janet_string_length(str));
  JanetString ret = janet_cstring(output);

  curl_easy_cleanup(curl);
  curl_free(output);

  return janet_wrap_string(ret);
}

static Janet cfun_unescape(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 1);
  JanetString str = janet_getstring(argv, 0);

  CURL *curl = curl_easy_init();
  if (!curl)
    janet_panic("cannot get curl handle");

  int outlength;
  char *output = curl_easy_unescape(curl, (const char *)str,
                                    janet_string_length(str), &outlength);
  JanetString ret = janet_string((const unsigned char *)output, outlength);

  curl_easy_cleanup(curl);
  curl_free(output);

  return janet_wrap_string(ret);
}

static const JanetReg cfuns[] = {
    {"request", cfun_request,
     "(cjequests/request method url &opt data headers)\n\nPerform an HTTP "
     "request to the specified url."},
    {"escape", cfun_escape,
     "(cjequests/escape str)\n\nURL escape the input string."},
    {"unescape", cfun_unescape,
     "(cjequests/unescape str)\n\nURL unescape the input string."},
    {NULL, NULL, NULL}};

JANET_MODULE_ENTRY(JanetTable *env) {
  janet_cfuns(env, "cjequests", cfuns);
  if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
    janet_panic("cannot initialize curl");
  }
}
