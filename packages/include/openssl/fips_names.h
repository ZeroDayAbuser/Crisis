








#ifndef OPENSSL_FIPS_NAMES_H
#define OPENSSL_FIPS_NAMES_H
#pragma once

#ifdef __cplusplus
extern "C" {
#endif









#define OSSL_PROV_FIPS_PARAM_MODULE_MAC "module-mac"

#define OSSL_PROV_FIPS_PARAM_INSTALL_VERSION "install-version"

#define OSSL_PROV_FIPS_PARAM_INSTALL_MAC "install-mac"

#define OSSL_PROV_FIPS_PARAM_INSTALL_STATUS "install-status"






#define OSSL_PROV_FIPS_PARAM_CONDITIONAL_ERRORS "conditional-errors"





#define OSSL_PROV_FIPS_PARAM_DEFER_TESTS "defer-tests"


#define OSSL_PROV_FIPS_PARAM_SECURITY_CHECKS OSSL_PROV_PARAM_SECURITY_CHECKS
#define OSSL_PROV_FIPS_PARAM_TLS1_PRF_EMS_CHECK OSSL_PROV_PARAM_TLS1_PRF_EMS_CHECK
#define OSSL_PROV_FIPS_PARAM_DRBG_TRUNC_DIGEST OSSL_PROV_PARAM_DRBG_TRUNC_DIGEST

#ifdef __cplusplus
}
#endif

#endif 
