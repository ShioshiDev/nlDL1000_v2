# NovaLogic DL1000 - Project Overview

## LED STATUS OVERVIEW

** Left LED - Device Status **
        DEVICE_STARTED - Green
        DEVICE_UPDATING - Blue  
        DEVICE_UPDATE_FAILED - Red

** Mid LED - Generator Data Status **

** Right LED - Networking Status **
    - Networking Status
        NETWORK_STOPPED - Grey (default)
        NETWORK_STARTED - White
        NETWORK_DISCONNECTED - Orange
        NETWORK_LOST_IP - Grey
        NETWORK_CONNECTED - Yellow
        NETWORK_CONNECTED_IP - (depends on connectivity & services status)

    - Connectivity Status (when NETWORK_CONNECTED_IP)
        CONNECTIVITY_OFFLINE - Violet (default)
        CONNECTIVITY_CHECKING - Light Blue
        CONNECTIVITY_ONLINE - (depends on services status)

    - Services Status (when CONNECTIVITY_ONLINE)
        SERVICES_STOPPED - Dark Blue (default)
        SERVICES_STARTING - Blue
        SERVICES_CONNECTING - Purple
        SERVICES_CONNECTED - Green
        SERVICES_ERROR - Red
        SERVICES_NOT_CONNECTED - Dark Blue
