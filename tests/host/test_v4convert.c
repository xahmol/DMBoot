/*
Host test: v4_convert_slot / v4_convert_config on real v4 files.
Usage: test_v4convert <dmbootconf.prg> <DMBCFGFILE or -> <slots.out>
Writes the v5 slot file and prints the converted settings for
run_tests.py to compare with the Python reference converter.
*/
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "../../src/petconv.c"
#include "../../src/cfgdefaults.c"
#include "../../src/v4convert.c"

static char v4slots[V4_LOADADDR_BYTES + V4_SLOTS_BYTES];
static char v4cfg[V4CFG_SIZE];

int main(int argc, char **argv)
{
    struct SlotStruct slot;
    struct ConfigStruct config;
    FILE *f;
    int havecfg = 0;

    if (argc != 4 || sizeof(struct SlotStruct) != SLOTSIZE || sizeof(struct ConfigStruct) != CONFIGSIZE)
    {
        fprintf(stderr, "usage or struct size error: slot %zu config %zu\n",
                sizeof(struct SlotStruct), sizeof(struct ConfigStruct));
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f || fread(v4slots, 1, sizeof(v4slots), f) != sizeof(v4slots))
    {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    fclose(f);
    if (strcmp(argv[2], "-") != 0)
    {
        f = fopen(argv[2], "rb");
        havecfg = f && fread(v4cfg, 1, sizeof(v4cfg), f) == sizeof(v4cfg);
        if (f)
        {
            fclose(f);
        }
    }

    f = fopen(argv[3], "wb");
    for (int x = 0; x < SLOTS; x++)
    {
        v4_convert_slot(v4slots + V4_LOADADDR_BYTES + x * V4_SLOT_STRIDE, &slot);
        fwrite(&slot, 1, sizeof(slot), f);
    }
    fclose(f);

    int defaulted = v4_convert_config(v4cfg, havecfg, "/usb*/11/", &config);
    printf("havecfg=%d defaulted=%d\n", havecfg, defaulted);
    printf("version=%d timeon=%d utc=%d verbose=%d host=%s|%s|%s\n", config.version, config.timeon,
           config.secondsfromutc, config.verbose, config.host, config.host2, config.host3);
    printf("reu=%s|%s size=%d\n", config.geos.reu_path, config.geos.reu_image, config.geos.reusize);
    printf("a=%d|%s|%s\n", config.geos.image_a_id, config.geos.image_a_path, config.geos.image_a_file);
    printf("b=%d|%s|%s\n", config.geos.image_b_id, config.geos.image_b_path, config.geos.image_b_file);
    return 0;
}
