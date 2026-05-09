#pragma once

#include <string>
#include <vector>

#include "envoy/ssl/context.h"

#include "source/common/common/utility.h"

#include "absl/types/optional.h"
#include "openssl/ssl.h"
#include "openssl/x509v3.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Tls {
namespace Utility {

/******************
gm
sign
xxxxxxxxxxxxx
enc
xxxxxxxxxxxxx
******************/

class CertificateParser {
public:
    struct GmCertificates {
        std::string sign;  // 签名证书/密钥
        std::string enc;   // 加密证书/密钥
        bool is_gm = false;     // 是否国密
    };

    static GmCertificates parse(const std::string& input) {
        GmCertificates result;
        std::string trimmed = trim(input);
        
        if (trimmed.empty()) {
            return result;
        }
        
        if (isGmFormat(trimmed)) {
            result.is_gm = true;
            parseGmFormat(trimmed, result);
        } else {
            // 非国密格式，整个字符串就是证书/密钥
            result.is_gm = false;
            result.sign = trimmed;
            result.enc = trimmed; 
        }
        
        return result;
    }

private:
    static bool isGmFormat(const std::string& str) {
        std::string lower_str = toLower(str);
        return lower_str.substr(0, 2) == "gm";
    }
    
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            return "";
        }
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    static void parseGmFormat(const std::string& input, GmCertificates& result) {
        std::vector<std::string> lines;
        splitLines(input, lines);
        
        std::string current_section;
        std::string sign_content;
        std::string enc_content;
        
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string line = trim(lines[i]);
            
            if (line.empty()) {
                continue;
            }
            
            std::string lower_line = toLower(line);
            if (lower_line == "sign" || lower_line == "enc") {
                current_section = lower_line;
                continue;
            }
            
            if (current_section == "sign") {
                if (!sign_content.empty()) {
                    sign_content += "\n";
                }
                sign_content += line;
            } else if (current_section == "enc") {
                if (!enc_content.empty()) {
                    enc_content += "\n";
                }
                enc_content += line;
            }
        }
        
        result.sign = trim(sign_content);
        result.enc = trim(enc_content);
    }

    static void splitLines(const std::string& input, std::vector<std::string>& lines) {
        size_t start = 0;
        size_t end = input.find('\n');
        
        while (end != std::string::npos) {
            lines.push_back(input.substr(start, end - start));
            start = end + 1;
            end = input.find('\n', start);
        }
        
        if (start < input.length()) {
            lines.push_back(input.substr(start));
        }
    }
};


Envoy::Ssl::CertificateDetailsPtr certificateDetails(X509* cert, const std::string& path,
                                                     TimeSource& time_source);

/**
 * Determines whether the given name matches 'pattern' which may optionally begin with a wildcard.
 * @param dns_name the DNS name to match
 * @param pattern the pattern to match against (*.example.com)
 * @return true if the san matches pattern
 */
bool dnsNameMatch(absl::string_view dns_name, absl::string_view pattern);

/**
 * Retrieves the serial number of a certificate.
 * @param cert the certificate
 * @return std::string the serial number field of the certificate. Returns "" if
 *         there is no serial number.
 */
std::string getSerialNumberFromCertificate(X509& cert);

/**
 * Retrieves the subject alternate names of a certificate.
 * @param cert the certificate
 * @param type type of subject alternate name
 * @return std::vector returns the list of subject alternate names.
 */
std::vector<std::string> getSubjectAltNames(X509& cert, int type);

/**
 * Converts the Subject Alternate Name to string.
 * @param general_name the subject alternate name
 * @return std::string returns the string representation of subject alt names.
 */
std::string generalNameAsString(const GENERAL_NAME* general_name);

/**
 * Retrieves the issuer from certificate.
 * @param cert the certificate
 * @return std::string the issuer field for the certificate.
 */
std::string getIssuerFromCertificate(X509& cert);

/**
 * Retrieves the subject from certificate.
 * @param cert the certificate
 * @return std::string the subject field for the certificate.
 */
std::string getSubjectFromCertificate(X509& cert);

/**
 * Retrieves the value of a specific X509 extension from the cert, if present.
 * @param cert the certificate.
 * @param extension_name the name of the extension to extract in dotted number format
 * @return absl::string_view the DER-encoded value of the extension field or empty if not present.
 */
absl::string_view getCertificateExtensionValue(X509& cert, absl::string_view extension_name);

/**
 * Returns the days until this certificate is valid.
 * @param cert the certificate
 * @param time_source the time source to use for current time calculation.
 * @return the number of days till this certificate is valid.
 */
int32_t getDaysUntilExpiration(const X509* cert, TimeSource& time_source);

/**
 * Returns the time from when this certificate is valid.
 * @param cert the certificate.
 * @return time from when this certificate is valid.
 */
SystemTime getValidFrom(const X509& cert);

/**
 * Returns the time when this certificate expires.
 * @param cert the certificate.
 * @return time after which the certificate expires.
 */
SystemTime getExpirationTime(const X509& cert);

/**
 * Returns the last crypto error from ERR_get_error(), or `absl::nullopt`
 * if the error stack is empty.
 * @return std::string error message
 */
absl::optional<std::string> getLastCryptoError();

/**
 * Returns error string corresponding error code derived from OpenSSL.
 * @param err error code
 * @return string message corresponding error code.
 */
absl::string_view getErrorDescription(int err);

/**
 * Extracts the X509 certificate validation error information.
 *
 * @param ctx the store context
 * @return the error details
 */
std::string getX509VerificationErrorInfo(X509_STORE_CTX* ctx);

} // namespace Utility
} // namespace Tls
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
