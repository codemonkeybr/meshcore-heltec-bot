#pragma once

#include "Command.h"
#include "../BotMesh.h"
#include "../HttpWorker.h"

// Minute-level rain/snow nowcast with amount (Open-Meteo 15-minutely). `snow` and
// `precip` are aliases: the reply says what is actually falling either way.
class RainCommand : public Command {
public:
  const char* keyword() const override { return "rain"; }
  const char* help() const override { return "rain - rain/snow nowcast for my location (also: snow, precip)"; }

  bool matches(const BotMsg& msg) const override {
    return matchesKeyword(msg, "rain") || matchesKeyword(msg, "snow") ||
           matchesKeyword(msg, "precip");
  }

  void run(BotMesh& mesh, const BotMsg& msg) override {
    double lat, lon;
    if (!mesh.selfLocation(lat, lon)) { mesh.botReply(msg, "my location isn't set"); return; }
    HttpJob j;
    j.kind = HTTP_RAIN;
    j.target = mesh.targetFor(msg);
    j.lat = lat; j.lon = lon;
    if (!HttpWorker::instance().submit(j))
      mesh.botReply(msg, "busy fetching, try again shortly");
  }
};
