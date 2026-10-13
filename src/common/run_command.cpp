// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <common/run_command.h>

#include <tinyformat.h>
#include <univalue.h>

#ifdef ENABLE_EXTERNAL_SIGNER
#include <boost/process.hpp>
#endif // ENABLE_EXTERNAL_SIGNER

UniValue RunCommandParseJSON(const std::string& str_command, const std::string& str_std_in)
{
#ifdef ENABLE_EXTERNAL_SIGNER
    namespace bp = boost::process;

    UniValue result_json;
    bp::opstream stdin_stream;
    bp::ipstream stdout_stream;
    bp::ipstream stderr_stream;

    if (str_command.empty()) return UniValue::VNULL;

    bp::child c(
        str_command,
        bp::std_out > stdout_stream,
        bp::std_err > stderr_stream,
        bp::std_in < stdin_stream
    );
    if (!str_std_in.empty()) {
        stdin_stream << str_std_in << std::endl;
    }
    stdin_stream.pipe().close();

    std::string result;
    std::string error;
    std::getline(stdout_stream, result);
    std::getline(stderr_stream, error);

    c.wait();
    const int n_error = c.exit_code();
    if (n_error) throw std::runtime_error(strprintf("RunCommandParseJSON error: process(%s) returned %d: %s\n", str_command, n_error, error));
    if (!result_json.read(result)) throw std::runtime_error("Unable to parse JSON: " + result);

    return result_json;
#else
    throw std::runtime_error("Compiled without external signing support (required for external signing).");
#endif // ENABLE_EXTERNAL_SIGNER
}

// VALIDITY-PORT: argv-based overload. See header for rationale.
// Passing arguments as a vector to boost::process avoids the tokenizer
// entirely — each arg goes through to argv as a single entry, regardless
// of whitespace, quotes, or metacharacters. Use this overload whenever
// any command-line component comes from untrusted data.
UniValue RunCommandParseJSON(const std::string& exe,
                             const std::vector<std::string>& args,
                             const std::string& str_std_in)
{
#ifdef ENABLE_EXTERNAL_SIGNER
    namespace bp = boost::process;

    UniValue result_json;
    bp::opstream stdin_stream;
    bp::ipstream stdout_stream;
    bp::ipstream stderr_stream;

    if (exe.empty()) return UniValue::VNULL;

    // `exe` is the user-configured -signer value (e.g. "hwi" or "py -3 hwi.py"), not
    // untrusted data. Split it on whitespace (honouring double quotes) like the string
    // overload's tokenizer did, and resolve a bare program name through PATH, since
    // the argv form of bp::child does neither. Untrusted values stay in `args`.
    std::vector<std::string> cmd_parts;
    {
        std::string cur;
        bool in_quotes{false}, have_token{false};
        for (const char ch : exe) {
            if (ch == '"') {
                in_quotes = !in_quotes;
                have_token = true;
            } else if (!in_quotes && (ch == ' ' || ch == '\t')) {
                if (have_token) cmd_parts.push_back(cur);
                cur.clear();
                have_token = false;
            } else {
                cur += ch;
                have_token = true;
            }
        }
        if (have_token) cmd_parts.push_back(cur);
    }
    if (cmd_parts.empty()) return UniValue::VNULL;

    bp::filesystem::path program{cmd_parts[0]};
    if (!program.has_parent_path()) {
        const bp::filesystem::path found{bp::search_path(cmd_parts[0])};
        if (!found.empty()) program = found;
    }
    std::vector<std::string> full_args(cmd_parts.begin() + 1, cmd_parts.end());
    full_args.insert(full_args.end(), args.begin(), args.end());

    bp::child c(
        program,
        bp::args = full_args,
        bp::std_out > stdout_stream,
        bp::std_err > stderr_stream,
        bp::std_in < stdin_stream
    );
    if (!str_std_in.empty()) {
        stdin_stream << str_std_in << std::endl;
    }
    stdin_stream.pipe().close();

    std::string result;
    std::string error;
    std::getline(stdout_stream, result);
    std::getline(stderr_stream, error);

    c.wait();
    const int n_error = c.exit_code();
    if (n_error) {
        // Reconstruct a display-only command line for the error message.
        // NOT the actual command that ran — this is just for logging/debugging.
        std::string display_command = exe;
        for (const auto& arg : args) display_command += " " + arg;
        throw std::runtime_error(strprintf("RunCommandParseJSON error: process(%s) returned %d: %s\n", display_command, n_error, error));
    }
    if (!result_json.read(result)) throw std::runtime_error("Unable to parse JSON: " + result);

    return result_json;
#else
    throw std::runtime_error("Compiled without external signing support (required for external signing).");
#endif // ENABLE_EXTERNAL_SIGNER
}
