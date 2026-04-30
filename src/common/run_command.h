// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMMON_RUN_COMMAND_H
#define BITCOIN_COMMON_RUN_COMMAND_H

#include <string>
#include <vector>

class UniValue;

/**
 * Execute a command which returns JSON, and parse the result.
 *
 * WARNING: The single-string overload passes `str_command` to
 * boost::process, which splits on whitespace with quote-awareness.
 * Attacker-controlled data inside `str_command` can break out of the
 * intended argument boundaries and inject additional CLI flags into
 * the subprocess. Do NOT use this overload with any data that is not
 * fully under the node operator's control — use the argv-based
 * overload below instead.
 *
 * @param str_command The command to execute, including any arguments
 * @param str_std_in string to pass to stdin
 * @return parsed JSON
 */
UniValue RunCommandParseJSON(const std::string& str_command, const std::string& str_std_in="");

/**
 * Execute a command with explicit argv and parse its JSON output.
 *
 * Arguments are passed directly to the subprocess as argv, with no
 * shell or boost::process tokenizer involvement. Individual args may
 * safely contain whitespace, quotes, semicolons, or any other bytes
 * without interpretation. This is the injection-safe overload — use
 * it whenever any portion of the command line comes from peer input,
 * wallet data, external subprocesses (e.g., hardware wallet responses
 * via HWI), or any other source that is not fully trusted.
 *
 * @param exe Path to the executable (also not tokenized — must be a single path)
 * @param args Vector of argv entries (each passed as a single argument)
 * @param str_std_in string to pass to stdin
 * @return parsed JSON
 */
UniValue RunCommandParseJSON(const std::string& exe,
                             const std::vector<std::string>& args,
                             const std::string& str_std_in="");

#endif // BITCOIN_COMMON_RUN_COMMAND_H
