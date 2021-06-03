/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "BraveCertificatePublicKeyInfo.h"
#if defined(BRAVE_CORE) // Compiling in Brave-Core
  #import "brave/ios/browser/api/networking/common/brave_certificate_enums.h"
  #include "brave/ios/browser/api/networking/utils/brave_certificate_ios_utils.h"
  #include "brave/ios/browser/api/networking/utils/brave_certificate_utils.h"
#else
  #import "brave_certificate_enums.h"
  #include "brave_certificate_ios_utils.h"
  #include "brave_certificate_utils.h"
#endif

@implementation BraveCertificatePublicKeyInfo
- (instancetype)initWithCertificate:(X509*)certificate {
  if ((self = [super init])) {
    _type = BravePublicKeyType_UNKNOWN;
    _keyUsage = BraveKeyUsage_INVALID;
    _algorithm = [[NSString alloc] init];
    _objectIdentifier = [[NSString alloc] init];
    _curveName = [[NSString alloc] init];
    _nistCurveName = [[NSString alloc] init];
    _parameters = [[NSString alloc] init];
    _keyHexEncoded = [[NSString alloc] init];
    
    // Sometimes a key cannot be "decoded"
    // but you can still use X509_get_X509_PUBKEY to get its raw form
    EVP_PKEY* key = X509_get_pubkey(certificate);
    if (key) {
      _keySizeInBits = EVP_PKEY_bits(key);
      _keyBytesSize = _keySizeInBits / 8;
      int key_type = EVP_PKEY_base_id(key);
      
      switch (key_type) {
        case EVP_PKEY_RSA: {
          _type = BravePublicKeyType_RSA;
          const RSA* rsa_key = EVP_PKEY_get0_RSA(key);
          if (rsa_key) {
            const BIGNUM* n = nullptr;
            const BIGNUM* e = nullptr;
            const BIGNUM* d = nullptr;
            RSA_get0_key(rsa_key, &n, &e, &d);
            if (n && e) {
              _exponent = BN_get_word(e);
              //_keySizeInBits = BN_num_bits(n);
              _keyHexEncoded = brave::string_to_ns(x509_utils::hex_string_from_BIGNUM(n));  // The modulus is the raw key
            }
          }
        }
          break;
        case EVP_PKEY_DSA: {
          _type = BravePublicKeyType_DSA;
          const DSA* dsa_key = EVP_PKEY_get0_DSA(key);
          if (dsa_key) {
            const BIGNUM* p = nullptr;
            const BIGNUM* q = nullptr;
            const BIGNUM* g = nullptr;
            DSA_get0_pqg(dsa_key, &p, &q, &g);
            if (p) {
              _keySizeInBits = BN_num_bits(p);
            }
            
            const BIGNUM* pub_key = nullptr;
            const BIGNUM* priv_key = nullptr;
            DSA_get0_key(dsa_key, &pub_key, &priv_key);  // Decode the key by removing its ASN1 sequence
            if (pub_key) {
              _keyHexEncoded = brave::string_to_ns(x509_utils::hex_string_from_BIGNUM(pub_key));
            }
          }
        }
          break;
        case EVP_PKEY_DH: {
          _type = BravePublicKeyType_DH;
          const DH* dh_key = EVP_PKEY_get0_DH(key);
          if (dh_key) {
            const BIGNUM* p = nullptr;
            const BIGNUM* q = nullptr;
            const BIGNUM* g = nullptr;
            DH_get0_pqg(dh_key, &p, &q, &g);
            if (p) {
              _keySizeInBits = BN_num_bits(p);
            }
            
            const BIGNUM* pub_key = nullptr;
            const BIGNUM* priv_key = nullptr;
            DH_get0_key(dh_key, &pub_key, &priv_key);  // Decode the key by removing its ASN1 sequence
            if (pub_key) {
              _keyHexEncoded = brave::string_to_ns(x509_utils::hex_string_from_BIGNUM(pub_key));
            }
          }
        }
          break;
        case EVP_PKEY_EC: {
          _type = BravePublicKeyType_EC;
          const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key);
          if (ec_key) {
            const EC_GROUP* group = EC_KEY_get0_group(ec_key);
            if (group) {
              //_keySizeInBits = EC_GROUP_get_degree(group); // EC_GROUP_order_bits //BN_num_bytes(EC_GROUP_get0_order());
              
              int name_nid = EC_GROUP_get_curve_name(group);
              _curveName = brave::string_to_ns(OBJ_nid2sn(name_nid));
              _nistCurveName = brave::string_to_ns(
                                                x509_utils::EC_curve_nid2nist(name_nid));
              
              #ifndef OPENSSL_IS_BORINGSSL  // BoringSSL missing support for EC_GROUP_get_point_conversion_form
              unsigned char* ec_pub_key = nullptr;
              std::size_t length = EC_KEY_key2buf(ec_key, EC_GROUP_get_point_conversion_form(group), &ec_pub_key, nullptr);
              if (ec_pub_key) {
                if (length > 0) {
                  std::vector<std::uint8_t> buffer = std::vector<std::uint8_t>(length);
                  std::memcpy(&buffer[0], ec_pub_key, length);
                  _keyHexEncoded = brave::string_to_ns(x509_utils::hex_string_from_bytes(buffer));
                }
                OPENSSL_free(ec_pub_key);
              }
              #endif
            }
          }
        }
          break;
      }
      EVP_PKEY_free(key);
    }
    
    // X509_PUB_KEY->public_key != EVP_PKEY
    if (![_keyHexEncoded length]) {
      // When the key type cannot be decoded, the best we can do is display the ASN1 DER encoded key as hexadecimal
      ASN1_BIT_STRING* key_bits = X509_get0_pubkey_bitstr(certificate);
      if (key_bits) {
        _keyHexEncoded = brave::string_to_ns(
                                      x509_utils::hex_string_from_ASN1_BIT_STRING(key_bits));
      }
    }
    
    X509_PUBKEY* pub_key = X509_get_X509_PUBKEY(certificate);
    if (pub_key) {
      ASN1_OBJECT* object = nullptr;
      X509_ALGOR* palg = nullptr;
      X509_PUBKEY_get0_param(&object, nullptr, nullptr, &palg, pub_key);
      
      if (palg) {
        // same as x509_utils::string_from_ASN1_OBJECT(object, false)
        _algorithm = brave::string_to_ns(
                                  x509_utils::string_from_x509_algorithm(palg));
      }
      
      if (object) {
        _objectIdentifier = brave::string_to_ns(
                                 x509_utils::string_from_ASN1_OBJECT(object, true));
      }
      
      int param_type = 0;
      const void* param = nullptr;
      X509_ALGOR_get0(nullptr, &param_type, &param, palg);
      
      if (param) {
        if (param_type == V_ASN1_SEQUENCE) {
          const ASN1_STRING* seq_string = static_cast<const ASN1_STRING*>(param);
          _parameters = brave::string_to_ns(
                                         x509_utils::hex_string_from_ASN1STRING(seq_string));
        }
        
        // Possible ECC certificate & Unsupported algorithms
        if (object && !key) {
          // Retrieve public key info
          int algorithm_key_type = OBJ_obj2nid(object);
          switch (algorithm_key_type) {
            case NID_rsaEncryption: {
              _type = BravePublicKeyType_RSA;
            }
              break;
            case NID_dsa: {
              _type = BravePublicKeyType_DSA;
            }
              break;
            case NID_dhpublicnumber:{
              _type = BravePublicKeyType_DH;
            }
              break;
            case NID_X9_62_id_ecPublicKey: {
              _type = BravePublicKeyType_EC;
              
              if (param_type == V_ASN1_SEQUENCE) {
                const ASN1_STRING* sequence = reinterpret_cast<const ASN1_STRING*>(param);
                _curveName = brave::string_to_ns(
                                              x509_utils::string_from_ASN1STRING(sequence));
                //BoringSSL doesn't let us parse this, but technically, we can use:
                //Works in OpenSSL though
                //EC_KEY* ec_key = d2i_ECParameters(nullptr, &data, length);
                //For now, leave it
              } else if (param_type == V_ASN1_OBJECT) {
                const ASN1_OBJECT* object = reinterpret_cast<const ASN1_OBJECT*>(param);
                _curveName = brave::string_to_ns(
                                              x509_utils::string_from_ASN1_OBJECT(object, false));
                int nid = OBJ_obj2nid(object);
                _nistCurveName = brave::string_to_ns(
                                                  x509_utils::EC_curve_nid2nist(nid));
                _keySizeInBits = x509_utils::EC_curve_nid2num_bits(nid);
                _keyBytesSize = _keySizeInBits / 8;

                //BoringSSL would fail to create a group based on the nid
                //Works in OpenSSL though
                //EC_GROUP* group = EC_GROUP_new_by_curve_name(nid);
                //For now, leave it
              }
            }
              break;
              
            case V_ASN1_UNDEF: {
              _type = BravePublicKeyType_UNKNOWN;
            }
              break;
              
            default: {
              _type = BravePublicKeyType_UNKNOWN;
            }
              break;
          }
        }
      }
    }
    
    //Alternative to d2i is:
    //int key_usage = X509_get_key_usage(certificate)
    //But if we use it, it would require manual bit-toggling
    ASN1_BIT_STRING* key_usage = static_cast<ASN1_BIT_STRING*>(
                                                 X509_get_ext_d2i(certificate,
                                                                  NID_key_usage,
                                                                  nullptr,
                                                                  nullptr));
    
    if (key_usage) {
      _keyUsage = brave::convert_key_usage(key_usage);
      ASN1_BIT_STRING_free(key_usage);
    } else {
      //encrypt, verify, derive, wrap
      _keyUsage = BravePublicKeyUsage_ANY;
    }
  }
  return self;
}

- (bool)isValidRSAKey:(RSA*)rsa_key {
  const BIGNUM* n = nullptr;
  const BIGNUM* e = nullptr;
  RSA_get0_key(rsa_key, &n, &e, nullptr);
  
  uint64_t exponent = BN_get_word(e);
  int modulus = BN_num_bits(n);
  
  if (exponent != 3 && exponent != 65537) {
    return false;
  }
  
  if (modulus != 1024 && modulus != 2048 && modulus != 3072 && modulus != 4096 && modulus != 8192) {
    return false;
  }
  return true;
}

- (NSString *)getECCurveName:(EC_KEY*)ec_key {
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);
  if (group) {
    int name_nid = EC_GROUP_get_curve_name(group);
    return brave::string_to_ns(OBJ_nid2sn(name_nid));
  }
  return [[NSString alloc] init];
}
@end
