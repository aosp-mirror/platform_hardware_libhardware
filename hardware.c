/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <hardware/hardware.h>

#include <cutils/properties.h>

#include <dlfcn.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

#define LOG_TAG "HAL"
#include <utils/Log.h>

/** Base path of the hal modules */
#define HAL_LIBRARY_PATH "/system/lib/hw"

/**
 * There are a set of variant filename for modules. The form of the filename
 * is "<MODULE_ID>.variant.so" so for the led module the Dream variants 
 * of base "ro.product.board", "ro.board.platform" and "ro.arch" would be:
 *
 * led.trout.so
 * led.msm7k.so
 * led.ARMV6.so
 * led.default.so
 */

#define HAL_DEFAULT_VARIANT     "default"
static const char *variant_keys[] = {
    "ro.hardware",  /* This goes first so that it can pick up a different
                       file on the emulator. */
    "ro.product.board",
    "ro.board.platform",
    "ro.arch"
};
#define HAL_VARIANT_KEYS_COUNT  (sizeof(variant_keys)/sizeof(variant_keys[0]))

/**
 * Load the file defined by the variant and if succesfull
 * return the dlopen handle and the hmi.
 * @return 0 = success, !0 = failure.
 */
static int load(const char *id,
                const char *variant,
                const struct hw_module_t **pHmi)
{
    int status;
    void *handle;
    const struct hw_module_t *hmi;
    char path[PATH_MAX];

    /* Construct the path. */
    snprintf(path, sizeof(path), "%s/%s.%s.so", HAL_LIBRARY_PATH, id, variant);

    LOGV("load: E id=%s path=%s", id, path);

    /*
     * load the symbols resolving undefined symbols before
     * dlopen returns. Since RTLD_GLOBAL is not or'd in with
     * RTLD_NOW the external symbols will not be global
     */
    handle = dlopen(path, RTLD_NOW);
    if (handle == NULL) {
        char const *err_str = dlerror();
        LOGW("load: module=%s error=%s", path, err_str);
        status = -EINVAL;
        goto done;
    }

    /* Get the address of the struct hal_module_info. */
    const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
    hmi = (const struct hw_module_t *)dlsym(handle, sym);
    if (hmi == NULL) {
        char const *err_str = dlerror();
        LOGE("load: couldn't find symbol %s", sym);
        status = -EINVAL;
        goto done;
    }

    /* Check that the id matches */
    if (strcmp(id, hmi->id) != 0) {
        LOGE("load: id=%s != hmi->id=%s", id, hmi->id);
        status = -EINVAL;
        goto done;
    }

    /* success */
    status = 0;

done:
    if (status != 0) {
        hmi = NULL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
    }

    *pHmi = hmi;

    LOGV("load: X id=%s path=%s hmi=%p handle=%p status=%d",
         id, path, *pHmi, handle, status);
    return status;
}

int hw_get_module(const char *id, const struct hw_module_t **module) 
{
    int status;
    int i;
    const struct hw_module_t *hmi = NULL;
    char prop[PATH_MAX];

    /*
     * Here we rely on the fact that calling dlopen multiple times on
     * the same .so will simply increment a refcount (and not load
     * a new copy of the library).
     * We also assume that dlopen() is thread-safe.
     */
    
    LOGV("hal_module_info_get: Load module id=%s", id);

    status = -EINVAL;

    /* Loop through the configuration variants looking for a module */
    for (i = 0; (status != 0) && (i < HAL_VARIANT_KEYS_COUNT); i++) {
        if (property_get(variant_keys[i], prop, NULL) == 0) {
            continue;
        }
        status = load(id, prop, &hmi);
    }

    /* Try default */
    if (status != 0) {
        status = load(id, HAL_DEFAULT_VARIANT, &hmi);
    }
    
    *module = hmi;
    LOGV("hal_module_info_get: X id=%s hmi=%p status=%d", id, hmi, status);

    return status;
}
