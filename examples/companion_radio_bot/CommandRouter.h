#pragma once

#include "BotMsg.h"

class BotMesh;
class Command;

// Compile-time registry + matcher for bot commands. The command table is fixed
// at build time (see CommandRouter.cpp); there is no dynamic plugin loading.
class CommandRouter {
public:
  void begin();

  // Return the first command whose matches() accepts `msg`, or nullptr.
  // Does not execute — the caller applies rate limiting, then calls run().
  Command* match(const BotMsg& msg) const;

  int count() const { return _count; }

  // Every compiled-in command, including ones switched off in settings (the
  // settings page lists these; count()/at() only cover the enabled ones).
  static int totalCommands();
  static const char* commandKeyword(int i);

  // True if `text`, received as a message, would trigger any compiled-in command
  // (aliases included, settings ignored: another bot may have different ones on).
  static bool wouldTrigger(const char* text);
  Command* at(int i) const { return (i >= 0 && i < _count) ? _cmds[i] : nullptr; }

private:
  static const int MAX_COMMANDS = 48;
  Command* _cmds[MAX_COMMANDS];
  int _count = 0;
};
