#include "apostol/application.hpp"
#include "apostol/http.hpp"
#include "apostol/module.hpp"
#ifdef WITH_POSTGRESQL
#include "apostol/pg.hpp"
#endif
#include "apostol/websocket.hpp"

#include "Modules.hpp"
#include "Processes.hpp"

using namespace apostol;

class CSApp final : public Application
{
public:
    CSApp() : Application("cs") {}

protected:
    void create_custom_processes() override
    {
        create_processes(*this);
    }

    void on_worker_start(EventLoop& loop) override
    {
#ifdef WITH_POSTGRESQL
        const auto& conninfo = settings().pg_conninfo_worker;
        if (!conninfo.empty()) {
            setup_db(loop, conninfo,
                static_cast<std::size_t>(settings().pg_pool_min),
                static_cast<std::size_t>(settings().pg_pool_max));
        }
#endif
        create_workers(*this);
        start_http_server(loop, settings().server_port);
        logger().info("worker ready (pid={}, port={})", ::getpid(), http_port());
    }
};

int main(int argc, char* argv[])
{
    CSApp app;
    app.set_info(APP_NAME, APP_VERSION, APP_DESCRIPTION);
    return app.run(argc, argv);
}
