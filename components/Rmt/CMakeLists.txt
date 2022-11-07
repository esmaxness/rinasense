idf_component_register(SRCS "rmt_ps_default.c" "Rmt.c" "rmt.c" "pci.c" "du.c"
    INCLUDE_DIRS "include"
    REQUIRES IPCP configRINA configSensor EFCP)
