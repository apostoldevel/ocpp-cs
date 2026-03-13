#pragma once
#include "apostol/application.hpp"

#ifdef WITH_POSTGRESQL
#include "CPEmulator/CPEmulator.hpp"
#endif

namespace apostol
{
static inline void create_processes(Application& app)
{
#ifdef WITH_POSTGRESQL
    if (app.module_enabled("ChargePoint", false))
        app.add_custom_process(std::make_unique<CPEmulator>());
#endif
    (void)app;
}
} // namespace apostol
