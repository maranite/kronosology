/*
 * OmapVideoMod.c - module load/unload glue for OmapVideoModule.ko.
 *
 * Reconstructed to match the original's file grouping: the MODULE_*
 * metadata macros and the platform_driver/platform_device registration
 * glue (module_init/module_exit targets) live in this translation unit,
 * matching the ".symtab" FILE marker "OmapVideoMod.c" seen in the
 * original binary (only __mod_version/__mod_author/__mod_description/
 * __mod_license are grouped under that marker there; init_module/
 * cleanup_module/omapfb_init/omapfb_exit are GLOBAL-bound symbols whose
 * originating file can't be told apart from the symtab alone, but this
 * is the conventional place for them and the logic clearly belongs
 * with the MODULE_* declarations).
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include "omapvideo_internal.h"

MODULE_AUTHOR("Korg R&D");
MODULE_DESCRIPTION("Korg OMAP Video interface Support");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

int omapfb_init(void)
{
	int ret;

	ret = platform_driver_register(&omapfb_driver);
	if (ret)
		return ret;

	omapfb_device = platform_device_alloc("omapfb", -1);
	if (!omapfb_device) {
		ret = -ENOMEM;
		goto err_unregister_driver;
	}

	ret = platform_device_add(omapfb_device);
	if (ret)
		goto err_put_device;

	ret = OmapVideoProcInitialize();
	return ret;

err_put_device:
	platform_device_put(omapfb_device);
err_unregister_driver:
	platform_driver_unregister(&omapfb_driver);
	return ret;
}

void omapfb_exit(void)
{
	if (gProc)
		OmapVideoProcDone();

	platform_device_unregister(omapfb_device);
	platform_driver_unregister(&omapfb_driver);
}

module_init(omapfb_init);
module_exit(omapfb_exit);
