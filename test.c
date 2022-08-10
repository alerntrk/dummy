#include <linux/module.h>
#include <linux/platform_device.h>

struct platform_device platform_pcdev_1 = {
    .name = "pseudo-char-device",
    .id = 0;

}
struct platform_device platform_pcdev_2 = {
    .name = "pseudo-char-device",
    .id = 1;

}


static int __init pcdev_platform_init(void){


    platform_device_register(&platform_pcdev_1);
    platform_device_register(&platform_pcdev_2);
    return 0;


}
static void  __exit pcdev_platform_exit(void){
    platform_device_unregister(&platform_pcdev_1);
    platform_device_unregister(&platform_pcdev_2);

}
module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("asdnajsd");
