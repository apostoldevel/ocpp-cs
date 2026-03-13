#pragma once
#include "apostol/application.hpp"
#include "CSService/CSService.hpp"

namespace apostol
{
static inline void create_workers(Application& app)
{
    if (app.module_enabled("CSService"))
        app.module_manager().add_module(std::make_unique<CSService>(app));
}
} // namespace apostol
