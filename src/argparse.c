#include "ldk/argparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


void ldkArgInitParser(LDKArgParser *parser)
{
  parser->argCount = 0;
  memset(parser->args, 0, LDK_ARGPARSE_MAX_ARGS * sizeof(LDKArgument));
}


void ldkArgAddArgument(LDKArgParser *parser, const char *name, LDKArgType type, const char *description)
{
  if (parser->argCount < LDK_ARGPARSE_MAX_ARGS)
  {
    LDKArgument *arg = &parser->args[parser->argCount++];
    arg->name = name;
    arg->type = type;
    arg->description = description;
    arg->isFound = false; // Newly added argument is not found yet
  } else
  {
    fprintf(stderr, "Error: Maximum number of arguments reached (%d)\n", LDK_ARGPARSE_MAX_ARGS);
    exit(EXIT_FAILURE); // Exit if trying to add too many arguments
  }
}


void ldkArgPrintUsage(const LDKArgParser *parser)
{
  printf("Usage:\n");
  for (int i = 0; i < parser->argCount; i++)
  {
    const LDKArgument *arg = &parser->args[i];

    printf("  %s", arg->name);

    int spacesToAlign = 24 - (int)strlen(arg->name);
    if (spacesToAlign > 0)
    {
      printf("%*s", spacesToAlign, " ");
    }

    printf("%s\n", arg->description);
  }
}


void ldkArgParseArguments(LDKArgParser *parser, uint32 argc, char *argv[])
{
  for (uint32 i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
    {
      // Display usage message and exit if -h or --help is encountered
      ldkArgPrintUsage(parser);
      exit(0);
    }

    // Parse other arguments
    for (uint32 j = 0; j < parser->argCount; j++)
    {
      LDKArgument *arg = &parser->args[j];
      if (strcmp(argv[i], arg->name) == 0)
      {
        arg->isFound = true;  // Mark the argument as found
        switch (arg->type)
        {
          case LDK_ARG_TYPE_FLAG:
            arg->flagValue = true;
            break;
          case LDK_ARG_TYPE_INT:
            if (i + 1 < argc)
            {
              arg->intValue = atoi(argv[++i]);
            }
            break;
          case LDK_ARG_TYPE_STRING:
            if (i + 1 < argc)
            {
              arg->stringValue = argv[++i];
            }
            break;
        }
      }
    }
  }
}


bool ldkArgGetFlagValue(const LDKArgParser *parser, const char *name)
{
  for (uint32 i = 0; i < parser->argCount; i++)
  {
    if (strcmp(parser->args[i].name, name) == 0 && parser->args[i].type == LDK_ARG_TYPE_FLAG)
    {
      return parser->args[i].flagValue;
    }
  }
  return false;
}


int32 ldkArgGetIntValue(const LDKArgParser *parser, const char *name)
{
  for (uint32 i = 0; i < parser->argCount; i++)
  {
    if (strcmp(parser->args[i].name, name) == 0 && parser->args[i].type == LDK_ARG_TYPE_INT)
    {
      return parser->args[i].intValue;
    }
  }
  return 0;
}


const char* ldkArgGetStringValue(const LDKArgParser *parser, const char *name)
{
  for (uint32 i = 0; i < parser->argCount; i++)
  {
    if (strcmp(parser->args[i].name, name) == 0 && parser->args[i].type == LDK_ARG_TYPE_STRING)
    {
      return parser->args[i].stringValue;
    }
  }
  return NULL;
}


void ldkArgForEachArgument(const LDKArgParser *parser, void (*callback)(const LDKArgument *arg))
{
  for (uint32 i = 0; i < parser->argCount; i++)
  {
    if (parser->args[i].isFound)
    {
      callback(&parser->args[i]);
    }
  }
}



/*
   int32 main(int32 argc, char *argv[])
   {
   LDKArgParser parser;
   ldkArgInitParser(&parser);

   ldkArgAddArgument(&parser, "-flag", ARG_TYPE_FLAG, "A simple boolean flag");
   ldkArgAddArgument(&parser, "-myIntValue", ARG_TYPE_INT, "An integer value for processing");
   ldkArgAddArgument(&parser, "-myStringValue", ARG_TYPE_STRING, "A string input");

   ldkArgParseArguments(&parser, argc, argv);

   printf("Flag: %s\n", ldkArgGetFlagValue(&parser, "-flag") ? "true" : "false");
   printf("Integer Value: %d\n", ldkArgGetIntValue(&parser, "-myIntValue"));
   const char *stringValue = ldkArgGetStringValue(&parser, "-myStringValue");
   printf("String Value: %s\n", stringValue ? stringValue : "(none)");

   printf("\n--- Found Arguments ---\n");
   ldkArgForEachArgument(&parser, printFoundArgument);  // Print32each found argument

// Print32usage if no arguments were found
if (argc == 1)
{
ldkArgPrintUsage(&parser);
}

ldkArgFreeParser(&parser);
return 0;
}
*/
