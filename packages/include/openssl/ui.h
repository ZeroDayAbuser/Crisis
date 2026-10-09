





#ifndef OPENSSL_UI_H
#define OPENSSL_UI_H
#pragma once

#include <openssl/macros.h>
#ifndef OPENSSL_NO_DEPRECATED_3_0
#define HEADER_UI_H
#endif

#include <openssl/opensslconf.h>

#ifndef OPENSSL_NO_DEPRECATED_1_1_0
#include <openssl/crypto.h>
#endif
#include <openssl/safestack.h>
#include <openssl/pem.h>
#include <openssl/types.h>
#include <openssl/uierr.h>


#ifndef OPENSSL_NO_DEPRECATED_3_0
#ifdef OPENSSL_NO_UI_CONSOLE
#define OPENSSL_NO_UI
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif




UI *UI_new(void);
UI *UI_new_method(const UI_METHOD *method);
void UI_free(UI *ui);


int UI_add_input_string(UI *ui, const char *prompt, int flags,
    char *result_buf, int minsize, int maxsize);
int UI_dup_input_string(UI *ui, const char *prompt, int flags,
    char *result_buf, int minsize, int maxsize);
int UI_add_verify_string(UI *ui, const char *prompt, int flags,
    char *result_buf, int minsize, int maxsize,
    const char *test_buf);
int UI_dup_verify_string(UI *ui, const char *prompt, int flags,
    char *result_buf, int minsize, int maxsize,
    const char *test_buf);
int UI_add_input_boolean(UI *ui, const char *prompt, const char *action_desc,
    const char *ok_chars, const char *cancel_chars,
    int flags, char *result_buf);
int UI_dup_input_boolean(UI *ui, const char *prompt, const char *action_desc,
    const char *ok_chars, const char *cancel_chars,
    int flags, char *result_buf);
int UI_add_info_string(UI *ui, const char *text);
int UI_dup_info_string(UI *ui, const char *text);
int UI_add_error_string(UI *ui, const char *text);
int UI_dup_error_string(UI *ui, const char *text);



#define UI_INPUT_FLAG_ECHO 0x01

#define UI_INPUT_FLAG_DEFAULT_PWD 0x02


#define UI_INPUT_FLAG_USER_BASE 16


char *UI_construct_prompt(UI *ui_method,
    const char *phrase_desc, const char *object_name);


void *UI_add_user_data(UI *ui, void *user_data);

int UI_dup_user_data(UI *ui, void *user_data);

void *UI_get0_user_data(UI *ui);


const char *UI_get0_result(UI *ui, int i);
int UI_get_result_length(UI *ui, int i);


int UI_process(UI *ui);


int UI_ctrl(UI *ui, int cmd, long i, void *p, void (*f)(void));



#define UI_CTRL_PRINT_ERRORS 1

#define UI_CTRL_IS_REDOABLE 2


#define UI_set_app_data(s, arg) UI_set_ex_data(s, 0, arg)
#define UI_get_app_data(s) UI_get_ex_data(s, 0)

#define UI_get_ex_new_index(l, p, newf, dupf, freef) \
    CRYPTO_get_ex_new_index(CRYPTO_EX_INDEX_UI, l, p, newf, dupf, freef)
int UI_set_ex_data(UI *r, int idx, void *arg);
void *UI_get_ex_data(const UI *r, int idx);


void UI_set_default_method(const UI_METHOD *meth);
const UI_METHOD *UI_get_default_method(void);
const UI_METHOD *UI_get_method(UI *ui);
const UI_METHOD *UI_set_method(UI *ui, const UI_METHOD *meth);

#ifndef OPENSSL_NO_UI_CONSOLE


UI_METHOD *UI_OpenSSL(void);

#endif


const UI_METHOD *UI_null(void);





typedef struct ui_string_st UI_STRING;


SKM_DEFINE_STACK_OF_INTERNAL(UI_STRING, UI_STRING, UI_STRING)
#define sk_UI_STRING_num(sk) OPENSSL_sk_num(ossl_check_const_UI_STRING_sk_type(sk))
#define sk_UI_STRING_value(sk, idx) ((UI_STRING *)OPENSSL_sk_value(ossl_check_const_UI_STRING_sk_type(sk), (idx)))
#define sk_UI_STRING_new(cmp) ((STACK_OF(UI_STRING) *)OPENSSL_sk_set_cmp_thunks(OPENSSL_sk_new(ossl_check_UI_STRING_compfunc_type(cmp)), sk_UI_STRING_cmpfunc_thunk))
#define sk_UI_STRING_new_null() ((STACK_OF(UI_STRING) *)OPENSSL_sk_set_thunks(OPENSSL_sk_new_null(), sk_UI_STRING_freefunc_thunk))
#define sk_UI_STRING_new_reserve(cmp, n) ((STACK_OF(UI_STRING) *)OPENSSL_sk_set_cmp_thunks(OPENSSL_sk_new_reserve(ossl_check_UI_STRING_compfunc_type(cmp), (n)), sk_UI_STRING_cmpfunc_thunk))
#define sk_UI_STRING_reserve(sk, n) OPENSSL_sk_reserve(ossl_check_UI_STRING_sk_type(sk), (n))
#define sk_UI_STRING_free(sk) OPENSSL_sk_free(ossl_check_UI_STRING_sk_type(sk))
#define sk_UI_STRING_zero(sk) OPENSSL_sk_zero(ossl_check_UI_STRING_sk_type(sk))
#define sk_UI_STRING_delete(sk, i) ((UI_STRING *)OPENSSL_sk_delete(ossl_check_UI_STRING_sk_type(sk), (i)))
#define sk_UI_STRING_delete_ptr(sk, ptr) ((UI_STRING *)OPENSSL_sk_delete_ptr(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_type(ptr)))
#define sk_UI_STRING_push(sk, ptr) OPENSSL_sk_push(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_type(ptr))
#define sk_UI_STRING_unshift(sk, ptr) OPENSSL_sk_unshift(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_type(ptr))
#define sk_UI_STRING_pop(sk) ((UI_STRING *)OPENSSL_sk_pop(ossl_check_UI_STRING_sk_type(sk)))
#define sk_UI_STRING_shift(sk) ((UI_STRING *)OPENSSL_sk_shift(ossl_check_UI_STRING_sk_type(sk)))
#define sk_UI_STRING_pop_free(sk, freefunc) OPENSSL_sk_pop_free(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_freefunc_type(freefunc))
#define sk_UI_STRING_insert(sk, ptr, idx) OPENSSL_sk_insert(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_type(ptr), (idx))
#define sk_UI_STRING_set(sk, idx, ptr) ((UI_STRING *)OPENSSL_sk_set(ossl_check_UI_STRING_sk_type(sk), (idx), ossl_check_UI_STRING_type(ptr)))
#define sk_UI_STRING_find(sk, ptr) OPENSSL_sk_find(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_type(ptr))
#define sk_UI_STRING_find_ex(sk, ptr) OPENSSL_sk_find_ex(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_type(ptr))
#define sk_UI_STRING_find_all(sk, ptr, pnum) OPENSSL_sk_find_all(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_type(ptr), pnum)
#define sk_UI_STRING_sort(sk) OPENSSL_sk_sort(ossl_check_UI_STRING_sk_type(sk))
#define sk_UI_STRING_is_sorted(sk) OPENSSL_sk_is_sorted(ossl_check_const_UI_STRING_sk_type(sk))
#define sk_UI_STRING_dup(sk) ((STACK_OF(UI_STRING) *)OPENSSL_sk_dup(ossl_check_const_UI_STRING_sk_type(sk)))
#define sk_UI_STRING_deep_copy(sk, copyfunc, freefunc) ((STACK_OF(UI_STRING) *)OPENSSL_sk_deep_copy(ossl_check_const_UI_STRING_sk_type(sk), ossl_check_UI_STRING_copyfunc_type(copyfunc), ossl_check_UI_STRING_freefunc_type(freefunc)))
#define sk_UI_STRING_set_cmp_func(sk, cmp) ((sk_UI_STRING_compfunc)OPENSSL_sk_set_cmp_func(ossl_check_UI_STRING_sk_type(sk), ossl_check_UI_STRING_compfunc_type(cmp)))




enum UI_string_types {
    UIT_NONE = 0,
    UIT_PROMPT, 
    UIT_VERIFY, 
    UIT_BOOLEAN, 
    UIT_INFO, 
    UIT_ERROR 
};


UI_METHOD *UI_create_method(const char *name);
void UI_destroy_method(UI_METHOD *ui_method);
int UI_method_set_opener(UI_METHOD *method, int (*opener)(UI *ui));
int UI_method_set_writer(UI_METHOD *method,
    int (*writer)(UI *ui, UI_STRING *uis));
int UI_method_set_flusher(UI_METHOD *method, int (*flusher)(UI *ui));
int UI_method_set_reader(UI_METHOD *method,
    int (*reader)(UI *ui, UI_STRING *uis));
int UI_method_set_closer(UI_METHOD *method, int (*closer)(UI *ui));
int UI_method_set_data_duplicator(UI_METHOD *method,
    void *(*duplicator)(UI *ui, void *ui_data),
    void (*destructor)(UI *ui, void *ui_data));
int UI_method_set_prompt_constructor(UI_METHOD *method,
    char *(*prompt_constructor)(UI *ui,
        const char
            *phrase_desc,
        const char
            *object_name));
int UI_method_set_ex_data(UI_METHOD *method, int idx, void *data);
int (*UI_method_get_opener(const UI_METHOD *method))(UI *);
int (*UI_method_get_writer(const UI_METHOD *method))(UI *, UI_STRING *);
int (*UI_method_get_flusher(const UI_METHOD *method))(UI *);
int (*UI_method_get_reader(const UI_METHOD *method))(UI *, UI_STRING *);
int (*UI_method_get_closer(const UI_METHOD *method))(UI *);
char *(*UI_method_get_prompt_constructor(const UI_METHOD *method))(UI *, const char *, const char *);
void *(*UI_method_get_data_duplicator(const UI_METHOD *method))(UI *, void *);
void (*UI_method_get_data_destructor(const UI_METHOD *method))(UI *, void *);
const void *UI_method_get_ex_data(const UI_METHOD *method, int idx);




enum UI_string_types UI_get_string_type(UI_STRING *uis);

int UI_get_input_flags(UI_STRING *uis);

const char *UI_get0_output_string(UI_STRING *uis);

const char *UI_get0_action_string(UI_STRING *uis);

const char *UI_get0_result_string(UI_STRING *uis);
int UI_get_result_string_length(UI_STRING *uis);

const char *UI_get0_test_string(UI_STRING *uis);

int UI_get_result_minsize(UI_STRING *uis);

int UI_get_result_maxsize(UI_STRING *uis);

int UI_set_result(UI *ui, UI_STRING *uis, const char *result);
int UI_set_result_ex(UI *ui, UI_STRING *uis, const char *result, int len);


int UI_UTIL_read_pw_string(char *buf, int length, const char *prompt,
    int verify);
int UI_UTIL_read_pw(char *buf, char *buff, int size, const char *prompt,
    int verify);
UI_METHOD *UI_UTIL_wrap_read_pem_callback(pem_password_cb *cb, int rwflag);

#ifdef __cplusplus
}
#endif
#endif
