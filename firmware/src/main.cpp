#if defined(PPGFW_NETWORK_DIAGNOSTICS)
#include "app/network_diagnostics_app.h"
#elif defined(PPGFW_BOARD_DIAGNOSTICS)
#include "app/board_diagnostics_app.h"
#elif defined(PPGFW_OFFLINE_FIXTURE)
#include "app/offline_fixture_app.h"
#else
#include "app/app_controller.h"
#endif

namespace {
#if defined(PPGFW_NETWORK_DIAGNOSTICS)
ppgfw::NetworkDiagnosticsApp app;
#elif defined(PPGFW_BOARD_DIAGNOSTICS)
ppgfw::BoardDiagnosticsApp app;
#elif defined(PPGFW_OFFLINE_FIXTURE)
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
