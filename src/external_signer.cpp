// Copyright (c) 2018-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <common/run_command.h>
#include <core_io.h>
#include <psbt.h>
#include <util/strencodings.h>
#include <external_signer.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

ExternalSigner::ExternalSigner(const std::string& command, const std::string chain, const std::string& fingerprint, const std::string name): m_command(command), m_chain(chain), m_fingerprint(fingerprint), m_name(name) {}

// VALIDITY-PORT: Injection-safe argv builder shared by all external signer
// subprocess calls. m_fingerprint and m_chain come from parsing HWI's (or
// the hardware wallet's) JSON responses and are therefore not fully trusted;
// passing them as individual argv entries via the vector-based
// RunCommandParseJSON overload prevents a malicious fingerprint or chain
// string from closing a quoted argument and injecting additional CLI flags
// into HWI. Do NOT switch back to string concatenation + the single-string
// RunCommandParseJSON overload.
std::vector<std::string> ExternalSigner::CommonArgs() const
{
    return {"--fingerprint", m_fingerprint, "--chain", m_chain};
}

bool ExternalSigner::Enumerate(const std::string& command, std::vector<ExternalSigner>& signers, const std::string chain)
{
    // Call <command> enumerate
    const UniValue result = RunCommandParseJSON(command, std::vector<std::string>{"enumerate"});
    if (!result.isArray()) {
        throw std::runtime_error(strprintf("'%s' received invalid response, expected array of signers", command));
    }
    for (const UniValue& signer : result.getValues()) {
        // Check for error
        const UniValue& error = signer.find_value("error");
        if (!error.isNull()) {
            if (!error.isStr()) {
                throw std::runtime_error(strprintf("'%s' error", command));
            }
            throw std::runtime_error(strprintf("'%s' error: %s", command, error.getValStr()));
        }
        // Check if fingerprint is present
        const UniValue& fingerprint = signer.find_value("fingerprint");
        if (fingerprint.isNull()) {
            throw std::runtime_error(strprintf("'%s' received invalid response, missing signer fingerprint", command));
        }
        const std::string& fingerprintStr{fingerprint.get_str()};
        // Skip duplicate signer
        bool duplicate = false;
        for (const ExternalSigner& signer : signers) {
            if (signer.m_fingerprint.compare(fingerprintStr) == 0) duplicate = true;
        }
        if (duplicate) break;
        std::string name;
        const UniValue& model_field = signer.find_value("model");
        if (model_field.isStr() && model_field.getValStr() != "") {
            name += model_field.getValStr();
        }
        signers.emplace_back(command, chain, fingerprintStr, name);
    }
    return true;
}

UniValue ExternalSigner::DisplayAddress(const std::string& descriptor) const
{
    std::vector<std::string> args = CommonArgs();
    args.emplace_back("displayaddress");
    args.emplace_back("--desc");
    args.emplace_back(descriptor);
    return RunCommandParseJSON(m_command, args);
}

UniValue ExternalSigner::GetDescriptors(const int account)
{
    std::vector<std::string> args = CommonArgs();
    args.emplace_back("getdescriptors");
    args.emplace_back("--account");
    args.emplace_back(strprintf("%d", account));
    return RunCommandParseJSON(m_command, args);
}

bool ExternalSigner::SignTransaction(PartiallySignedTransaction& psbtx, std::string& error)
{
    // Serialize the PSBT
    CDataStream ssTx(SER_NETWORK);
    ssTx << psbtx;
    // parse ExternalSigner master fingerprint
    std::vector<unsigned char> parsed_m_fingerprint = ParseHex(m_fingerprint);
    // Check if signer fingerprint matches any input master key fingerprint
    auto matches_signer_fingerprint = [&](const PSBTInput& input) {
        for (const auto& entry : input.hd_keypaths) {
            if (parsed_m_fingerprint == MakeUCharSpan(entry.second.fingerprint)) return true;
        }
        for (const auto& entry : input.m_tap_bip32_paths) {
            if (parsed_m_fingerprint == MakeUCharSpan(entry.second.second.fingerprint)) return true;
        }
        return false;
    };

    if (!std::any_of(psbtx.inputs.begin(), psbtx.inputs.end(), matches_signer_fingerprint)) {
        error = "Signer fingerprint " + m_fingerprint + " does not match any of the inputs:\n" + EncodeBase64(ssTx.str());
        return false;
    }

    // VALIDITY-PORT: Build argv directly. The PSBT base64 goes through stdin, not argv,
    // so it can't inject into the command line regardless of its content. The
    // fingerprint and chain come from the argv-safe CommonArgs() helper.
    std::vector<std::string> args = {"--stdin"};
    auto common = CommonArgs();
    args.insert(args.end(), common.begin(), common.end());
    const std::string stdinStr = "signtx \"" + EncodeBase64(ssTx.str()) + "\"";

    const UniValue signer_result = RunCommandParseJSON(m_command, args, stdinStr);

    if (signer_result.find_value("error").isStr()) {
        error = signer_result.find_value("error").get_str();
        return false;
    }

    if (!signer_result.find_value("psbt").isStr()) {
        error = "Unexpected result from signer";
        return false;
    }

    PartiallySignedTransaction signer_psbtx;
    std::string signer_psbt_error;
    if (!DecodeBase64PSBT(signer_psbtx, signer_result.find_value("psbt").get_str(), signer_psbt_error)) {
        error = strprintf("TX decode failed %s", signer_psbt_error);
        return false;
    }

    psbtx = signer_psbtx;

    return true;
}
