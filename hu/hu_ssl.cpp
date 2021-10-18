#define MR_SSL_INTERNAL  // For certificate and private key only
#define LOGTAG "hu_ssl"
#include "hu_aap.h"
#include "hu_uti.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <pthread.h>
#include "hu_ssl.h"

using namespace AndroidAuto;

void HUServer::logSSLInfo() {
    // logd ("SSL_is_init_finished(): %d", SSL_is_init_finished (hu_ssl_ssl));

    const char *ssl_state_string_long =
        SSL_state_string_long(m_ssl);  // "SSLv3 write client hello B"
    logd("ssl_state_string_long: %s", ssl_state_string_long);

    const char *ssl_version = SSL_get_version(m_ssl);  // "TLSv1.2"
    logd("ssl_version: %s", ssl_version);

    const SSL_CIPHER *ssl_cipher = SSL_get_current_cipher(m_ssl);
    const char *ssl_cipher_name = SSL_CIPHER_get_name(ssl_cipher);  // "(NONE)"
    logd("ssl_cipher_name: %s", ssl_cipher_name);
}

void HUServer::logSSLReturnCode(int ret) {
    int ssl_err = SSL_get_error(m_ssl, ret);
    const char *err_str = "";

    switch (ssl_err) {
        case SSL_ERROR_NONE:
            err_str = ("");
            break;
        case SSL_ERROR_ZERO_RETURN:
            err_str = ("Error Zero Return");
            break;
        case SSL_ERROR_WANT_READ:
            err_str = ("Error Want Read");
            break;
        case SSL_ERROR_WANT_WRITE:
            err_str = ("Error Want Write");
            break;
        case SSL_ERROR_WANT_CONNECT:
            err_str = ("Error Want Connect");
            break;
        case SSL_ERROR_WANT_ACCEPT:
            err_str = ("Error Want Accept");
            break;
        case SSL_ERROR_WANT_X509_LOOKUP:
            err_str = ("Error Want X509 Lookup");
            break;
        case SSL_ERROR_SYSCALL:
            err_str = ("Error Syscall");
            break;
        case SSL_ERROR_SSL:
            err_str = ("Error SSL");
            break;
        default:
            err_str = ("Error Unknown");
            break;
    }

    ERR_print_errors_fp(stderr);

    if (strlen(err_str) == 0)
        logd("ret: %d  ssl_err: %d (Success)", ret, ssl_err);
    else
        loge("ret: %d  ssl_err: %d (%s)", ret, ssl_err, err_str);
}

int HUServer::sendSSLHandshakePacket() {
    byte hs_buf[MAX_FRAME_SIZE] = {0};

    int len = BIO_read(
        m_sslReadBio, hs_buf,
        sizeof(
            hs_buf));  // Read from the BIO Client request: Hello/Key Exchange
    if (len <= 0) {
        loge("BIO_read() HS client req ret: %d", len);
        return (-1);
    }
    logd("BIO_read() HS client req ret: %d", len);

    int ret =
        sendUnencodedBlob(0, ControlChannel,
                          HU_INIT_MESSAGE::SSLHandshake, hs_buf, len, 5000);
    if (ret < 0) {
        loge("hu_aap_tra_send() HS client req ret: %d  len: %d", ret, len);
        return -1;
    }

    return (0);
}

int HUServer::beginSSLHandshake() {
    int ret;
    BIO *cert_bio = NULL;
    BIO *pkey_bio = NULL;

    ret = SSL_library_init();  // Init
    logd("SSL_library_init ret: %d", ret);
    if (ret !=
        1) {  // Always returns "1", so it is safe to discard the return value.
        loge("SSL_library_init() error");
        return (-1);
    }

    SSL_load_error_strings();  // Before or after init ?
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    ERR_load_SSL_strings();

    OPENSSL_add_all_algorithms_noconf();  // Add all algorithms, without using
                                          // config file

    ret = RAND_status();  // 1 if the PRNG has been seeded with enough data, 0
                          // otherwise.
    logd("RAND_status ret: %d", ret);
    if (ret != 1) {
        loge("RAND_status() error");
        return (-1);
    }

    cert_bio = BIO_new_mem_buf(
        cert_buf, sizeof(cert_buf));  // Read only memory BIO for certificate
    pem_password_cb *ppcb1 = NULL;
    void *u1 = NULL;
    X509 *x509 = NULL;
    X509 *x509_cert = PEM_read_bio_X509_AUX(cert_bio, &x509, ppcb1, u1);
    if (x509_cert == NULL) {
        loge("read_bio_X509_AUX() error");
        return (-1);
    }
    logd("PEM_read_bio_X509_AUX() x509_cert: %p", x509_cert);
    ret = BIO_free(cert_bio);
    if (ret != 1)
        loge("BIO_free(cert_bio) ret: %d", ret);
    else
        logd("BIO_free(cert_bio) ret: %d", ret);

    pkey_bio = BIO_new_mem_buf(
        pkey_buf, sizeof(pkey_buf));  // Read only memory BIO for private key
    pem_password_cb *ppcb2 = NULL;
    void *u2 = NULL;
    // Read a private key from a BIO using a pass phrase callback:    key =
    // PEM_read_bio_PrivateKey(bp, NULL, pass_cb,  "My Private Key"); Read a
    // private key from a BIO using the pass phrase "hello":   key =
    // PEM_read_bio_PrivateKey(bp, NULL, 0,        "hello");
    EVP_PKEY *priv_key_ret = NULL;
    EVP_PKEY *priv_key =
        PEM_read_bio_PrivateKey(pkey_bio, &priv_key_ret, ppcb2, u2);
    if (priv_key == NULL) {
        loge("PEM_read_bio_PrivateKey() error");
        return (-1);
    }
    logd("PEM_read_bio_PrivateKey() priv_key: %p", priv_key);
    ret = BIO_free(pkey_bio);
    if (ret != 1)
        loge("BIO_free(pkey_bio) ret: %d", ret);
    else
        logd("BIO_free(pkey_bio) ret: %d", ret);

    m_sslMethod = (SSL_METHOD *)TLSv1_2_client_method();
    if (m_sslMethod == NULL) {
        loge("TLSv1_2_client_method() error");
        return (-1);
    }
    logd("TLSv1_2_client_method() hu_ssl_method: %p", m_sslMethod);

    m_sslContext = SSL_CTX_new(m_sslMethod);
    if (m_sslContext == NULL) {
        loge("SSL_CTX_new() error");
        return (-1);
    }
    logd("SSL_CTX_new() hu_ssl_ctx: %p", m_sslContext);

    ret = SSL_CTX_use_certificate(m_sslContext, x509_cert);
    if (ret != 1)
        loge("SSL_CTX_use_certificate() ret: %d", ret);
    else
        logd("SSL_CTX_use_certificate() ret: %d", ret);

    ret = SSL_CTX_use_PrivateKey(m_sslContext, priv_key);
    if (ret != 1)
        loge("SSL_CTX_use_PrivateKey() ret: %d", ret);
    else
        logd("SSL_CTX_use_PrivateKey() ret: %d", ret);

    // Must do all CTX setup before SSL_new() !!
    m_ssl = SSL_new(m_sslContext);
    if (m_ssl == NULL) {
        loge("SSL_new() hu_ssl_ssl: %p", m_ssl);
        return (-1);
    }
    logd("SSL_new() hu_ssl_ssl: %p", m_ssl);

    //	SSL_set_mode (hu_ssl_ssl, SSL_OP_NO_TLSv1|SSL_OP_NO_TLSv1_1
    //|SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);

    ret = SSL_check_private_key(m_ssl);
    if (ret != 1) {
        loge("SSL_check_private_key() ret: %d", ret);
        return (-1);
    }
    logd("SSL_check_private_key() ret: %d", ret);

    m_sslWriteBio = BIO_new(BIO_s_mem());
    if (m_sslWriteBio == NULL) {
        loge("BIO_new() hu_ssl_rm_bio: %p", m_sslWriteBio);
        return (-1);
    }
    logd("BIO_new() hu_ssl_rm_bio: %p", m_sslWriteBio);

    m_sslReadBio = BIO_new(BIO_s_mem());
    if (m_sslReadBio == NULL) {
        loge("BIO_new() hu_ssl_wm_bio: %p", m_sslReadBio);
        return (-1);
    }
    logd("BIO_new() hu_ssl_wm_bio: %p", m_sslReadBio);

    SSL_set_bio(m_ssl, m_sslWriteBio,
                m_sslReadBio);  // Read from memory, write to memory

    BIO_set_write_buf_size(
        m_sslWriteBio,
        MAX_FRAME_PAYLOAD_SIZE);  // BIO_ctrl() API to increase the buffer size.
    BIO_set_write_buf_size(m_sslReadBio, MAX_FRAME_PAYLOAD_SIZE);

    SSL_set_connect_state(m_ssl);  // Set ssl to work in client mode

    SSL_set_verify(m_ssl, SSL_VERIFY_NONE, NULL);

    ret = SSL_do_handshake(m_ssl);  // Do current handshake step processing
    logd("SSL_do_handshake() ret: %d", ret);

    // We should need input data
    if ((SSL_get_error(m_ssl, ret) == SSL_ERROR_WANT_READ)) {
        // keep going
        return sendSSLHandshakePacket();
    }

    return (-1);
}

int HUServer::handleSSLHandshake(byte *buf, int len) {
    int ret =
        BIO_write(m_sslWriteBio, buf, len);  // Write to the BIO Server response
    if (ret <= 0) {
        loge("BIO_write() server rsp ret: %d", ret);
        logSSLReturnCode(ret);
        logSSLInfo();
        //        g_free(hs_buf);
        return (-1);
    }
    logd("BIO_write() server rsp ret: %d", ret);

    ret = SSL_do_handshake(m_ssl);  // Do current handshake step processing
    logd("SSL_do_handshake() ret: %d", ret);

    if ((SSL_get_error(m_ssl, ret) == SSL_ERROR_WANT_READ)) {
        // keep going
        return sendSSLHandshakePacket();
    }

    if ((SSL_get_error(m_ssl, ret) != SSL_ERROR_NONE) ||
        !SSL_is_init_finished(m_ssl)) {
        logSSLReturnCode(ret);
        logSSLInfo();
        return (-1);
    }

    HU::AuthCompleteResponse response;
    response.set_status(HU::STATUS_OK);
    ret = sendUnencodedMessage(0, ControlChannel,
                               HU_INIT_MESSAGE::AuthComplete, response, 2000);
    if (ret < 0) {
        loge("hu_aap_unenc_send_message() ret: %d", ret);
        stop();
        return (-1);
    }
    logSSLInfo();

    iaap_state = hu_STATE_STARTED;
    logd("  SET: iaap_state: %d (%s)", iaap_state, state_get(iaap_state));

    return 0;
}
