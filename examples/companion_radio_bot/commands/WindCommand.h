#pragma once

#include "Command.h"
#include "../BotMesh.h"
#include "../HttpWorker.h"

class WindCommand : public Command {
public:
  const char* keyword() const override { return "wind"; }
  const char* help() const override { return "wind - current wind speed, direction and gusts for my location"; }

  void run(BotMesh& mesh, const BotMsg& msg) override {
    double lat, lon;
    if (!mesh.selfLocation(lat, lon)) { mesh.botReply(msg, "my location isn't set"); return; }
    HttpJob j;
    j.kind = HTTP_WIND;
    j.target = mesh.targetFor(msg);
    j.lat = lat; j.lon = lon;
    if (!HttpWorker::instance().submit(j))
      mesh.botReply(msg, "busy fetching, try again shortly");
  }
};
