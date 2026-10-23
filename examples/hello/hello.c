#include "proto_loaded.h"

int proto_loaded_main(const ProtoElfLoadMap_t *load_map,
                      const ProtoElfStartInfo_t *start_info) {
    (void)load_map;
    (void)start_info;
    void *memory = proto_malloc(64);
    if (memory == NULL) {
        proto_log(LOG_ERROR, "FAIL SDK hello: allocation refused");
        return -1;
    }
    proto_free(memory);
    proto_log(LOG_WARN, "PASS SDK hello: allocated, freed, returning normally");
    return 0;
}
