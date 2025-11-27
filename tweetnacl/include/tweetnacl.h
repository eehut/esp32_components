/**
 * @file tweetnacl.h
 * @brief TweetNaCl cryptographic library header
 * @note This is a minimal header for Ed25519 signing
 * 
 * TweetNaCl is a minimal cryptographic library.
 * For full implementation, download from: https://tweetnacl.cr.yp.to/
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// TweetNaCl constants for Ed25519
#define crypto_sign_BYTES 64
#define crypto_sign_PUBLICKEYBYTES 32
#define crypto_sign_SECRETKEYBYTES 64

/**
 * @brief Ed25519签名（detached模式，只返回签名，不包含消息）
 * 
 * @param sm 输出缓冲区（消息+签名，长度为mlen + crypto_sign_BYTES）
 * @param smlen 输出长度指针
 * @param m 要签名的消息
 * @param mlen 消息长度
 * @param sk 私钥（64字节，包含32字节私钥+32字节公钥）
 * @return 0 成功，-1 失败
 * 
 * @note TweetNaCl的crypto_sign会将消息和签名组合在一起
 * 对于JWT，我们需要detached签名（只有签名部分）
 * 可以使用crypto_sign_detached如果可用，否则需要从crypto_sign结果中提取
 */
int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long mlen,
                const unsigned char *sk);

/**
 * @brief Ed25519签名（detached模式，只返回签名）
 * 
 * @param sig 输出签名（64字节）
 * @param siglen 输出签名长度指针
 * @param m 要签名的消息
 * @param mlen 消息长度
 * @param sk 私钥（64字节）
 * @return 0 成功，-1 失败
 */
int crypto_sign_detached(unsigned char *sig, unsigned long long *siglen,
                         const unsigned char *m, unsigned long long mlen,
                         const unsigned char *sk);

/**
 * @brief 从32字节私钥推导出公钥
 * 
 * @param pk 输出公钥（32字节）
 * @param sk 输入私钥（32字节）
 * @return 0 成功，-1 失败
 */
int crypto_sign_publickey_from_secret(unsigned char *pk, const unsigned char *sk);

/**
 * @brief 从32字节私钥创建64字节密钥（32字节私钥 + 32字节公钥）
 * 
 * @param sk64 输出密钥（64字节）
 * @param sk32 输入私钥（32字节）
 * @return 0 成功，-1 失败
 */
int crypto_sign_secretkey_from_private(unsigned char *sk64, const unsigned char *sk32);

#ifdef __cplusplus
}
#endif

