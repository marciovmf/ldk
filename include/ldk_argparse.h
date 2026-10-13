/**
 * # Command-line arguments
 *
 * Command-line argument parsing
 *
 * A very simple Command-Line argument parser
 *
 * Features:
 * - Define expected command-line arguments with types and descriptions.
 * - Parse command-line input and automatically recognize flags and their associated values.
 * - Generate usage information based on argument descriptions for user guidance.
 * - Provides safe mechanisms to access argument values while preventing buffer overflows.
 *
 * Usage:
 * 1. Initialize the argument parser using ldkArgInitParser.
 * 2. Add expected arguments with ldkArgAddArgument.
 * 3. Parse command-line arguments using ldkArgParseArguments.
 * 4. Access argument values with the corresponding retrieval functions.
 * 5. Print usage information with ldkArgPrintUsage when needed.
 * 6. Iterate over found arguments using ldkArgForEachArgument.
 *
 *
 * Limitations:
 * - The number of arguments is intentionally fixed and determined at compile time.
 * - Currently, no dynamic memory management is implemented as arguments are stored in
 *   a statically allocated array.
 * - The size of the array is defined by the macro LDK_ARGPARSE_MAX_ARGS. The
 *   default value is 8. In case a larger value is needed, please define the
 *   macro with the required value before including this header.
 */

#ifndef LDK_ARGPARSE_H
#define LDK_ARGPARSE_H

#include <ldk_common.h>

/** @group core.argparse */
#define LDK_ARGPARSE_MAX_ARGS_DEFAULT 8

#ifndef LDK_ARGPARSE_MAX_ARGS 
/** @group core.argparse */
#define LDK_ARGPARSE_MAX_ARGS LDK_ARGPARSE_MAX_ARGS_DEFAULT 
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group core.argparse */
  typedef enum
  {
    LDK_ARG_TYPE_FLAG,
    LDK_ARG_TYPE_INT,
    LDK_ARG_TYPE_STRING
  } LDKArgType;

  /** @group core.argparse */
  typedef struct
  {
    const char *name;
    LDKArgType type;
    const char *description;
    char *stringValue;
    bool isFound;
    union
    {
      bool flagValue;
      i32 intValue;
    };
  } LDKArgument;

  /** @group core.argparse */
  typedef struct
  {
    LDKArgument args[LDK_ARGPARSE_MAX_ARGS];
    u32 argCount;
  } LDKArgParser; 

  /**
   * @brief Initialize an argument parser.
   * @param parser Argument parser to operate on.
   * @group core.argparse
   */
  LDK_API void ldkArgInitParser(LDKArgParser* parser);
  /**
   * @brief Register a named command-line argument with the parser.
   * @param parser Argument parser to operate on.
   * @param name Name identifying the object or setting.
   * @param type Type identifier or kind used by the operation.
   * @param description Description used by this operation.
   * @group core.argparse
   */
  LDK_API void ldkArgAddArgument(LDKArgParser* parser, const char* name, LDKArgType type, const char* description);
  /**
   * @brief Parse command-line arguments according to the registered options.
   * @param parser Argument parser to operate on.
   * @param argc Number of command-line arguments.
   * @param argv Array of command-line arguments.
   * @group core.argparse
   */
  LDK_API void ldkArgParseArguments(LDKArgParser* parser, u32 argc, char* argv[]);
  /**
   * @brief Print usage information for the registered command-line options.
   * @param parser Argument parser to operate on.
   * @group core.argparse
   */
  LDK_API void ldkArgPrintUsage(const LDKArgParser* parser);
  /**
   * @brief Read the value of a parsed command-line flag.
   * @param parser Argument parser to operate on.
   * @param name Name identifying the object or setting.
   * @return True if the operation succeeds; false otherwise.
   * @group core.argparse
   */
  LDK_API bool ldkArgGetFlagValue(const LDKArgParser* parser, const char* name);
  /**
   * @brief Read a parsed integer command-line argument.
   * @param parser Argument parser to operate on.
   * @param name Name identifying the object or setting.
   * @return The resulting integer value.
   * @group core.argparse
   */
  LDK_API i32 ldkArgGetIntValue(const LDKArgParser* parser, const char* name);
  /**
   * @brief Read a parsed string command-line argument.
   * @param parser Argument parser to operate on.
   * @param name Name identifying the object or setting.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group core.argparse
   */
  LDK_API const char* ldkArgGetStringValue(const LDKArgParser* parser, const char* name);
  /**
   * @brief Visit each registered argument with a callback.
   * @param parser Argument parser to operate on.
   * @param callback Callback invoked by this operation.
   * @group core.argparse
   */
  LDK_API void ldkArgForEachArgument(const LDKArgParser* parser, void (*callback)(const LDKArgument* arg));

#ifdef __cplusplus
}
#endif

#endif //LDK_ARGPARSE_H

