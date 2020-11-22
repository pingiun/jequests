#include <janet.h>
#include <curl/curl.h>

size_t write_to_buffer(void *buffer, size_t size, size_t nmemb, void *userp) {
	JanetBuffer *ret_buffer = (JanetBuffer *) userp;

	janet_buffer_push_bytes(ret_buffer, buffer, (int32_t) (size * nmemb));

	return size * nmemb;
}

static Janet cfun_request(int32_t argc, Janet *argv) {
	janet_fixarity(argc, 1);
	const char *url = janet_getcstring(argv, 0);

	CURL *curl = curl_easy_init();

	if (!curl) {
		janet_panic("cannot get curl handle");
	}

	CURLcode res;
	char errbuf[CURL_ERROR_SIZE];

	JanetBuffer *buffer = janet_buffer(0);
	JanetTable *ret_table = janet_table(2);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	// curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

	/* provide a buffer to store errors in */
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
	/* set the error buffer as empty before performing a request */
	errbuf[0] = 0;

	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		size_t len = strlen(errbuf);
		if (len) {
			janet_panicf("%s", errbuf);
		} else {
			janet_panicf("%s", curl_easy_strerror(res));
		}
	}

	long response_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	janet_table_put(ret_table, janet_ckeywordv("text"), janet_wrap_buffer(buffer));
	janet_table_put(ret_table, janet_ckeywordv("status-code"), janet_wrap_integer((int32_t) response_code));

	return janet_wrap_table(ret_table);
}

static const JanetReg cfuns[] = {
	{"request", cfun_request, "(jequests/request url)\n\nDoes a get request to the specified url."},
	{NULL, NULL, NULL}
};

extern const unsigned char *jequests_lib_embed;
extern size_t jequests_lib_embed_size;

JANET_MODULE_ENTRY(JanetTable *env) {
	janet_cfuns(env, "jequests", cfuns);
	janet_dobytes(env,
            jequests_lib_embed,
            jequests_lib_embed_size,
            "jequests_lib.janet",
            NULL);
	if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
		janet_panic("cannot initialize curl");
	}
}
