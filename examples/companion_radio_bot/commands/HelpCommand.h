#pragma once

#include "Command.h"
#include "../BotMesh.h"
#include "../CommandRouter.h"
#include <string.h>
#include <strings.h>

// "help" -> short overview ; "help <cmd>" -> that command's usage line.
class HelpCommand : public Command {
public:
  const char* keyword() const override { return "help"; }
  const char* help() const override { return "help [cmd] - this text, or usage for one command"; }

  void run(BotMesh& mesh, const BotMsg& msg) override {
    char topic[24];
    args(msg, topic, sizeof(topic));

    const CommandRouter& r = mesh.router();
    if (topic[0]) {
      for (int i = 0; i < r.count(); i++) {
        BotMsg q;                       // let aliases (help snow) find their command
        q.text = topic;
        if (r.at(i)->matches(q)) {
          const char* h = r.at(i)->help();
          char out[BOT_MAX_REPLY_LEN + 1];
          if (h && *h) snprintf(out, sizeof(out), "Usage: %s", h);   // keyword-first would re-trigger bots
          else         snprintf(out, sizeof(out), "no help for that command");
          mesh.botReply(msg, out);
          return;
        }
      }
      mesh.botReply(msg, "unknown command; try 'cmd'");
      return;
    }

    char out[BOT_MAX_REPLY_LEN + 1];
    snprintf(out, sizeof(out),
             "%s bot. 'cmd' lists commands, 'help <cmd>' for usage.",
             mesh.getNodeName());
    mesh.botReply(msg, out);
  }
};
