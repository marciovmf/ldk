/**
 * argparse.h
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

#include <stdbool.h>
#include "common.h"

#define LDK_ARGPARSE_MAX_ARGS_DEFAULT 8

#ifndef LDK_ARGPARSE_MAX_ARGS 
#define LDK_ARGPARSE_MAX_ARGS LDK_ARGPARSE_MAX_ARGS_DEFAULT 
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum
  {
    LDK_ARG_TYPE_FLAG,
    LDK_ARG_TYPE_INT,
    LDK_ARG_TYPE_STRING
  } LDKArgType;

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
      int32 intValue;
    };
  } LDKArgument;

  typedef struct
  {
    LDKArgument args[LDK_ARGPARSE_MAX_ARGS];
    uint32 argCount;
  } LDKArgParser; 

  LDK_API void ldkArgInitParser(LDKArgParser* parser);
  LDK_API void ldkArgAddArgument(LDKArgParser* parser, const char* name, LDKArgType type, const char* description);
  LDK_API void ldkArgParseArguments(LDKArgParser* parser, uint32 argc, char* argv[]);
  LDK_API void ldkArgPrintUsage(const LDKArgParser* parser);
  LDK_API bool ldkArgGetFlagValue(const LDKArgParser* parser, const char* name);
  LDK_API int32 ldkArgGetIntValue(const LDKArgParser* parser, const char* name);
  LDK_API const char* ldkArgGetStringValue(const LDKArgParser* parser, const char* name);
  LDK_API void ldkArgForEachArgument(const LDKArgParser* parser, void (*callback)(const LDKArgument* arg));

#ifdef __cplusplus
}
#endif

#endif //LDK_ARGPARSE_H

