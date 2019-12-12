/*
 *
 * Copyright 2019 Asylo authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "asylo/crypto/certificate_util.h"

#include <cstdint>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "asylo/crypto/x509_certificate.h"
#include "asylo/util/proto_enum_util.h"
#include "asylo/util/status_macros.h"

namespace asylo {

Status ValidateCertificateSigningRequest(const CertificateSigningRequest &csr) {
  if (!csr.has_format()) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        "CertificateSigningRequest missing required \"format\" field");
  }
  if (!csr.has_data()) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "CertificateSigningRequest missing required \"data\" field");
  }
  if (csr.format() == asylo::CertificateSigningRequest::UNKNOWN) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "CertificateSigningRequest has an unknown format");
  }

  return Status::OkStatus();
}

Status ValidateCertificate(const Certificate &certificate) {
  if (!certificate.has_format()) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "Certificate missing required \"format\" field");
  }
  if (!certificate.has_data()) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "Certificate missing required \"data\" field");
  }
  if (certificate.format() == asylo::Certificate::UNKNOWN) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "Certificate has an unknown format");
  }

  return Status::OkStatus();
}

Status ValidateCertificateChain(const CertificateChain &certificate_chain) {
  for (const auto &certificate : certificate_chain.certificates()) {
    ASYLO_RETURN_IF_ERROR(ValidateCertificate(certificate));
  }

  return Status::OkStatus();
}

Status ValidateCertificateRevocationList(const CertificateRevocationList &crl) {
  if (!crl.has_format()) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        "CertificateRevocationList missing required \"format\" field");
  }
  if (!crl.has_data()) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "CertificateRevocationList missing required \"data\" field");
  }
  if (crl.format() == asylo::CertificateRevocationList::UNKNOWN) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "CertificateRevocationList has an unknown format");
  }

  return Status::OkStatus();
}

StatusOr<std::unique_ptr<CertificateInterface>> CreateCertificateInterface(
    const CertificateFactoryMap &factory_map, const Certificate &certificate) {
  auto factory_iter = factory_map.find(certificate.format());
  if (factory_iter == factory_map.end()) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  absl::StrCat("No mapping from format to factory for format ",
                               ProtoEnumValueName(certificate.format())));
  }
  return (factory_iter->second)(certificate);
}

StatusOr<CertificateInterfaceVector> CreateCertificateChain(
    const CertificateFactoryMap &factory_map, const CertificateChain &chain) {
  CertificateInterfaceVector certificate_interface_chain;
  certificate_interface_chain.reserve(chain.certificates_size());
  for (int i = 0; i < chain.certificates_size(); i++) {
    const Certificate &cert = chain.certificates(i);
    auto factory_iter = factory_map.find(cert.format());
    if (factory_iter == factory_map.end()) {
      return Status(
          error::GoogleError::INVALID_ARGUMENT,
          absl::StrCat("At index ", i,
                       " no mapping from format to factory for format ",
                       Certificate_CertificateFormat_Name(cert.format())));
    }
    auto certificate_result = (factory_iter->second)(cert);
    if (!certificate_result.ok()) {
      return certificate_result.status().WithPrependedContext(
          absl::StrCat("Failed to create certificate at index ", i));
    }

    certificate_interface_chain.push_back(
        std::move(certificate_result).ValueOrDie());
  }
  return std::move(certificate_interface_chain);
}

Status VerifyCertificateChain(CertificateInterfaceSpan certificate_chain,
                              const VerificationConfig &verification_config) {
  if (certificate_chain.empty()) {
    return Status(error::GoogleError::INVALID_ARGUMENT,
                  "Certificate chain must include at least one certificate");
  }

  // For each certificate in the chain (except the root), verifies the
  // certificate with the issuer certificate. In the certificate list, the
  // issuer is the certificate next in the list after the certificate being
  // verified.
  int64_t ca_count = 0;
  for (int i = 0; i < certificate_chain.size() - 1; i++) {
    const CertificateInterface *subject = certificate_chain[i].get();
    const CertificateInterface *issuer = certificate_chain[i + 1].get();
    if (verification_config.max_pathlen) {
      absl::optional<int64_t> max_pathlength = issuer->CertPathLength();
      if (max_pathlength.has_value() && max_pathlength.value() < ca_count) {
        return asylo::Status(
            error::GoogleError::UNAUTHENTICATED,
            absl::StrCat(
                "Maximum pathlength of certificate at index ", i,
                " exceeded. Maximum pathlength: ", max_pathlength.value(),
                ", current pathlength: ", ca_count));
      }
    }
    absl::optional<bool> issuer_is_ca = issuer->IsCa();
    if (issuer_is_ca.value_or(true)) {
      ca_count++;
    }

    Status status = subject->Verify(*issuer, verification_config);
    if (!status.ok()) {
      return status.WithPrependedContext(
          absl::StrCat("Failed to verify certificate at index ", i));
    }
  }

  const CertificateInterface *root = certificate_chain.rbegin()->get();

  // Root certificate should be self-signed.
  Status status = root->Verify(*root, verification_config);
  if (!status.ok()) {
    return status.WithPrependedContext("Failed to verify root certificate");
  }

  return Status::OkStatus();
}

StatusOr<Certificate> GetCertificateFromPem(absl::string_view pem_cert) {
  std::unique_ptr<X509Certificate> cert;
  ASYLO_ASSIGN_OR_RETURN(cert, X509Certificate::CreateFromPem(pem_cert));
  return cert->ToCertificateProto(Certificate::X509_PEM);
}

StatusOr<CertificateChain> GetCertificateChainFromPem(
    absl::string_view pem_cert_chain) {
  constexpr absl::string_view kBeginCertLabel("-----BEGIN CERTIFICATE-----");
  constexpr absl::string_view kEndCertLabel("-----END CERTIFICATE-----");
  constexpr size_t kEndCertLen = kEndCertLabel.size();

  CertificateChain cert_chain;
  size_t cert_start = pem_cert_chain.find(kBeginCertLabel);
  size_t cert_end = pem_cert_chain.find(kEndCertLabel, cert_start);
  while ((cert_start != absl::string_view::npos) &&
         (cert_end != absl::string_view::npos)) {
    absl::string_view pem_cert =
        pem_cert_chain.substr(cert_start, cert_end + kEndCertLen - cert_start);
    ASYLO_ASSIGN_OR_RETURN(*cert_chain.add_certificates(),
                           GetCertificateFromPem(pem_cert));

    cert_start = pem_cert_chain.find(kBeginCertLabel, cert_end);
    cert_end = pem_cert_chain.find(kEndCertLabel, cert_start);
  }

  if (cert_chain.certificates_size() == 0) {
    return Status(
        error::GoogleError::INVALID_ARGUMENT,
        absl::StrCat(
            "The certificate chain string does not contain any pair of '",
            kBeginCertLabel, "' and '", kEndCertLabel, "'"));
  }
  return cert_chain;
}

StatusOr<CertificateRevocationList> GetCrlFromPem(absl::string_view pem_crl) {
  asylo::CertificateRevocationList crl_proto;
  crl_proto.set_format(asylo::CertificateRevocationList::X509_PEM);
  crl_proto.set_data(pem_crl.data(), pem_crl.length());
  return crl_proto;
}

}  // namespace asylo
