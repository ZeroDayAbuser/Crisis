#ifndef CURLINC_WEBSOCKETS_H
#define CURLINC_WEBSOCKETS_H

#ifdef __cplusplus
extern "C" {
#endif

struct curl_ws_frame {
  int age;
  int flags;
  curl_off_t offset;
  curl_off_t bytesleft;
  size_t len;
};

#define CURLWS_TEXT       (1 << 0)
#define CURLWS_BINARY     (1 << 1)
#define CURLWS_CONT       (1 << 2)
#define CURLWS_CLOSE      (1 << 3)
#define CURLWS_PING       (1 << 4)
#define CURLWS_OFFSET     (1 << 5)

CURL_EXTERN CURLcode curl_ws_recv(CURL *curl, void *buffer, size_t buflen,
                                  size_t *recv,
                                  const struct curl_ws_frame **metap);

#define CURLWS_PONG       (1 << 6)

CURL_EXTERN CURLcode curl_ws_send(CURL *curl, const void *buffer_arg,
                                  size_t buflen, size_t *sent,
                                  curl_off_t fragsize,
                                  unsigned int flags);

CURL_EXTERN CURLcode curl_ws_start_frame(CURL *curl,
                                         unsigned int flags,
                                         curl_off_t frame_len);

#define CURLWS_RAW_MODE   (1L << 0)
#define CURLWS_NOAUTOPONG (1L << 1)

CURL_EXTERN const struct curl_ws_frame *curl_ws_meta(CURL *curl);

#ifdef __cplusplus
}
#endif

#endif
