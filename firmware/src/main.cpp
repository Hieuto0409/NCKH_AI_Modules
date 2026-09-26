#if defined(PPGFW_OFFLINE_FIXTURE)
#include "app/offline_fixture_app.h"
#else
#include "app/app_controller.h"
#endif

namespace {
#if defined(PPGFW_OFFLINE_FIXTURE)
ppgfw::OfflineFixtureApp app;
#else
ppgfw::AppController app;
#endif
}

void setup() {
    app.begin();
}

void loop() {
    app.loop();
}
