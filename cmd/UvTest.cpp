#include <app/UvLoop.h>

#include "unistd.h"
using namespace os::app;

extern "C" int main() {
    os::app::UvLoop looper;
    os::app::UvLoop* handler = &looper;

    os::app::UvTimer* timer = new UvTimer();
    timer->init(looper.get(), [handler, timer](void*) {
        uv_print_all_handles(handler->get(), stderr);
        handler->stop();
    });
    timer->start(1000, 0);
    looper.run();
    timer->stop();
    timer->close();
    uv_print_all_handles(handler->get(), stderr);
    handler->stop();

    while (looper.isAlive()) {
        printf("looper is alive:%s\n", looper.isAlive() ? "true" : "false");
        // looper.close();
        uv_print_all_handles(handler->get(), stderr);
        looper.run(UV_RUN_NOWAIT);
        // looper.close();
        printf("looper is alive:%s\n", looper.isAlive() ? "true" : "false");
        sleep(1);
    }
    printf("looper is alive:%s\n", looper.isAlive() ? "true" : "false");
    int cnt = 5;
    while (0 != looper.close() && --cnt) {
        uv_print_all_handles(handler->get(), stderr);
        looper.run(UV_RUN_NOWAIT);
        sleep(1);
    }
    return 0;
}
