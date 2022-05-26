#ifndef __SERVICES_LIB_A32_H__
#define __SERVICES_LIB_A32_H__

#include "services_lib_public.h"
#include "services_lib_protocol.h"

/* Collective union with all the services */
typedef union {
    service_header_t service_header_t_v;
    get_rnd_svc_t get_rnd_svc_t_v;
    get_lcs_svc_t get_lcs_svc_t_v;
    mbedtls_crypto_init_svc_t mbedtls_crypto_init_svc_t_v;
    mbedtls_aes_set_key_svc_t mbedtls_aes_set_key_svc_t_v;
    mbedtls_aes_crypt_svc_t mbedtls_aes_crypt_svc_t_v;
    mbedtls_sha_svc_t mbedtls_sha_svc_t_v;
    mbedtls_ccm_set_key_svc_t mbedtls_ccm_set_key_svc_t_v;
    mbedtls_chacha20_crypt_svc_t mbedtls_chacha20_crypt_svc_t_v;
    mbedtls_chachapoly_crypt_svc_t mbedtls_chachapoly_crypt_svc_t_v;
    mbedtls_poly1305_crypt_svc_t mbedtls_poly1305_crypt_svc_t_v;
    mbedtls_trng_hardware_poll_svc_t mbedtls_trng_hardware_poll_svc_t_v;
    process_toc_entry_svc_t process_toc_entry_svc_t_v;
    boot_cpu_svc_t boot_cpu_svc_t_v;
    control_cpu_svc_t control_cpu_svc_t_v;
    pinmux_svc_t pinmux_svc_t_v;
    pad_control_svc_t pad_control_svc_t_v;
    uart_write_svc_t uart_write_svc_t_v;
    get_toc_version_svc_t get_toc_version_svc_t_v;
    get_toc_number_svc_t get_toc_number_svc_t_v;
    get_toc_via_name_svc_t get_toc_via_name_svc_t_v;
    get_toc_via_cpu_id_svc_t get_toc_via_cpu_id_svc_t_v;
    get_device_part_svc_t get_device_part_svc_t_v;
    set_services_capabilities_t set_services_capabilities_t_v;
    stop_mode_request_svc_t stop_mode_request_svc_t_v;
} all_services_svc_t;


void service_get_heartbeat(int, service_header_t *, uint32_t);
void service_get_rnd_num(int, get_rnd_svc_t *, uint32_t);
void service_get_lcs(int, get_lcs_svc_t *, uint32_t);
void service_get_toc_version(int, get_toc_version_svc_t *, uint32_t);
void service_get_toc_number(int, get_toc_number_svc_t *, uint32_t);
void service_uart_write(int, uart_write_svc_t *, uint32_t);
#endif

