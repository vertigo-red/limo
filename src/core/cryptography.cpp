#include "cryptography.h"
#include <memory>
#include <vector>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>


void throwError(const std::string& step)
{
  std::string error = "Error during " + step + ".\n";
  auto code = ERR_get_error();
  char buffer[256];
  while(code)
  {
    ERR_error_string(code, buffer);
    error.append(std::string(buffer));
    error.append("\n");
    code = ERR_get_error();
  }
  ERR_free_strings();
  throw CryptographyError(error);
}

namespace
{
using CipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

CipherCtxPtr makeCipherCtx()
{
  return CipherCtxPtr(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
}
} // namespace

namespace cryptography
{
std::tuple<std::string, std::string, std::string> encrypt(const std::string& plain_text,
                                                          const std::string& key)
{
  auto ctx = makeCipherCtx();
  if(!ctx)
    throwError("encryption");

  if(EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    throwError("encryption");

  constexpr int nonce_size = 12;
  std::vector<unsigned char> nonce(nonce_size);
  if(RAND_bytes(nonce.data(), nonce_size) != 1)
    throwError("encryption");

  std::string actual_key = key.empty() ? default_key : key;
  constexpr int key_size = 32;
  std::vector<unsigned char> key_padded(key_size);
  for(int i = 0; i < key_size; i++)
    key_padded[i] = actual_key[i % actual_key.size()];
  if(EVP_EncryptInit_ex(ctx.get(), NULL, NULL, key_padded.data(), nonce.data()) != 1)
    throwError("encryption");

  // GCM is a stream cipher, so ciphertext matches the plaintext length plus
  // room for the final block.
  std::vector<unsigned char> cipher_text(plain_text.size() + 16);
  int cur_length = 0;
  if(EVP_EncryptUpdate(ctx.get(),
                       cipher_text.data(),
                       &cur_length,
                       reinterpret_cast<const unsigned char*>(plain_text.data()),
                       plain_text.size()) != 1)
    throwError("encryption");

  int cipher_length = cur_length;
  if(EVP_EncryptFinal_ex(ctx.get(), cipher_text.data() + cur_length, &cur_length) != 1)
    throwError("encryption");
  cipher_length += cur_length;

  std::vector<unsigned char> tag(16);
  if(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1)
    throwError("encryption");

  const std::string cipher_str(reinterpret_cast<const char*>(cipher_text.data()), cipher_length);
  const std::string nonce_str(reinterpret_cast<const char*>(nonce.data()), nonce_size);
  const std::string tag_str(reinterpret_cast<const char*>(tag.data()), 16);

  return { cipher_str, nonce_str, tag_str };
}

std::string decrypt(const std::string& cipher_text,
                    const std::string& key,
                    const std::string& nonce,
                    const std::string& tag)
{
  auto ctx = makeCipherCtx();
  if(!ctx)
    throwError("decryption");

  if(EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    throwError("decryption");

  std::string actual_key = key.empty() ? default_key : key;
  constexpr int key_size = 32;
  std::vector<unsigned char> key_arr(key_size);
  for(int i = 0; i < key_size; i++)
    key_arr[i] = actual_key[i % actual_key.size()];
  std::vector<unsigned char> nonce_arr(nonce.begin(), nonce.end());
  if(EVP_DecryptInit_ex(ctx.get(), NULL, NULL, key_arr.data(), nonce_arr.data()) != 1)
    throwError("decryption");

  std::vector<unsigned char> cipher_arr(cipher_text.begin(), cipher_text.end());
  // GCM decrypts in place to at most the ciphertext length plus the final block.
  std::vector<unsigned char> plain_text(cipher_text.size() + 16);
  int cur_length = 0;
  if(EVP_DecryptUpdate(ctx.get(),
                       plain_text.data(),
                       &cur_length,
                       cipher_arr.data(),
                       cipher_text.size()) != 1)
    throwError("decryption");
  int plain_text_length = cur_length;

  std::vector<unsigned char> tag_arr(tag.begin(), tag.end());
  if(EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, tag_arr.data()) != 1)
    throwError("decryption");

  if(EVP_DecryptFinal_ex(ctx.get(), plain_text.data() + cur_length, &cur_length) <= 0)
    throwError("decryption");
  plain_text_length += cur_length;

  return std::string(reinterpret_cast<const char*>(plain_text.data()), plain_text_length);
}
} // namespace cryptography
