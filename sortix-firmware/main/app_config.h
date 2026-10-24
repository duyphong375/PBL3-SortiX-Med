#pragma once

// Keep the application and all firmware components on the same protocol and
// configuration definitions. The component-local header is the single source
// of truth and is exposed to main by the nvs_config dependency.
#include "../components/nvs_config/include/app_config.h"
