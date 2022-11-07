idf_component_register(SRCS "rfifo.c" "rstr.c" "ipcpIdm.c" "cepIdm.c" "pidm.c" "IpcManager.c" "normalIPCP.c" "IPCP.c" "common.c" "num_mgr.c" "rfifo.c"
    INCLUDE_DIRS "include"
    REQUIRES configSensor NetworkInterface ShimIPCP BufferManagement ARP826 Rmt RINA_API EFCP Enrollment Ribd FlowAllocator)
